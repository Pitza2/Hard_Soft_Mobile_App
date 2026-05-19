#include "core/CloudRunEventClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <time.h>

#include "config/AppConfig.h"

namespace {
constexpr uint32_t kJwtLifetimeSeconds = 3600;
constexpr uint32_t kIdTokenRefreshSkewSeconds = 60;
}

CloudRunEventClient::CloudRunEventClient(const char* serviceAccountEmail,
                                         const char* privateKeyId,
                                         const char* privateKeyPem,
                                         const char* cloudRunUrl)
    : serviceAccountEmail_(serviceAccountEmail),
      privateKeyId_(privateKeyId),
      privateKeyPem_(privateKeyPem),
      cloudRunUrl_(cloudRunUrl) {}

bool CloudRunEventClient::begin() {
  if (!hasValidConfig()) {
    Serial.println("Cloud Run notifier disabled: missing config");
    return false;
  }

  return true;
}

bool CloudRunEventClient::sendNotification(const String& message) {
  if (!hasValidConfig()) {
    return false;
  }

  String idToken;
  if (!ensureIdToken(idToken)) {
    return false;
  }

  return callCloudRun(idToken, buildNotificationBody(message));
}

bool CloudRunEventClient::ensureTimeSynced() {
  if (timeSynced_ && time(nullptr) > 1700000000) {
    return true;
  }

  configTime(0, 0, AppConfig::NTP_SERVER);
  const uint32_t startMs = millis();
  while (time(nullptr) < 1700000000 &&
         millis() - startMs < AppConfig::NTP_SYNC_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  timeSynced_ = time(nullptr) > 1700000000;
  if (!timeSynced_) {
    Serial.println("NTP sync failed");
  }

  return timeSynced_;
}

bool CloudRunEventClient::hasValidConfig() const {
  return serviceAccountEmail_ != nullptr && serviceAccountEmail_[0] != '\0' &&
         privateKeyId_ != nullptr && privateKeyId_[0] != '\0' &&
         privateKeyPem_ != nullptr && privateKeyPem_[0] != '\0' &&
         cloudRunUrl_ != nullptr && cloudRunUrl_[0] != '\0' &&
         String(serviceAccountEmail_).indexOf("replace-with-") == -1 &&
         String(privateKeyId_).indexOf("replace-with-") == -1 &&
         String(privateKeyPem_).indexOf("replace-with-") == -1 &&
         String(cloudRunUrl_).indexOf("replace-with-") == -1;
}

bool CloudRunEventClient::ensureIdToken(String& idToken) {
  if (!ensureTimeSynced()) {
    return false;
  }

  const time_t now = time(nullptr);
  if (!cachedIdToken_.isEmpty() &&
      now + kIdTokenRefreshSkewSeconds < idTokenExpiresAt_) {
    idToken = cachedIdToken_;
    return true;
  }

  String signedJwt;
  if (!createSignedJwt(signedJwt)) {
    Serial.println("Failed to create signed JWT");
    return false;
  }

  if (!exchangeJwtForIdToken(signedJwt, idToken)) {
    Serial.println("Failed to exchange JWT for ID token");
    return false;
  }

  cachedIdToken_ = idToken;
  idTokenExpiresAt_ = now + kJwtLifetimeSeconds;
  return true;
}

bool CloudRunEventClient::createSignedJwt(String& signedJwt) const {
  const time_t now = time(nullptr);
  const String header = buildJwtHeader();
  const String payload = buildJwtPayload(now);
  const String signingInput = header + "." + payload;

  String signatureBase64Url;
  if (!signJwt(signingInput, signatureBase64Url)) {
    return false;
  }

  signedJwt = signingInput + "." + signatureBase64Url;
  return true;
}

bool CloudRunEventClient::exchangeJwtForIdToken(const String& signedJwt,
                                                String& idToken) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, AppConfig::GOOGLE_OAUTH_TOKEN_URL)) {
    Serial.println("OAuth HTTP begin failed");
    return false;
  }

  http.setTimeout(AppConfig::CLOUD_RUN_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const String body =
      "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
      "&assertion=" +
      signedJwt;
  const int statusCode = http.POST(body);
  if (statusCode <= 0) {
    Serial.printf("OAuth POST failed: %s\n",
                  http.errorToString(statusCode).c_str());
    http.end();
    return false;
  }

  const String response = http.getString();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    Serial.printf("OAuth HTTP %d: %s\n", statusCode, response.c_str());
    return false;
  }

  idToken = extractJsonString(response, "id_token");
  if (idToken.isEmpty()) {
    Serial.println("OAuth response missing id_token");
    return false;
  }

  return true;
}

bool CloudRunEventClient::callCloudRun(const String& idToken,
                                       const String& body) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, cloudRunUrl_)) {
    Serial.println("Cloud Run HTTP begin failed");
    return false;
  }

  http.setTimeout(AppConfig::CLOUD_RUN_HTTP_TIMEOUT_MS);
  http.addHeader("Authorization", "Bearer " + idToken);
  http.addHeader("Content-Type", "application/json");
  const int statusCode = http.POST(body);
  const String response = http.getString();
  http.end();

  Serial.printf("Cloud Run response: HTTP %d\n", statusCode);
  Serial.println(response);

  if (statusCode <= 0) {
    return false;
  }

  return statusCode >= 200 && statusCode < 300;
}

String CloudRunEventClient::buildJwtHeader() const {
  const String json = String("{\"alg\":\"RS256\",\"typ\":\"JWT\",\"kid\":\"") +
                      privateKeyId_ + "\"}";
  return base64UrlEncode(reinterpret_cast<const uint8_t*>(json.c_str()),
                         json.length());
}

String CloudRunEventClient::buildJwtPayload(time_t now) const {
  const String json =
      String("{\"iss\":\"") + serviceAccountEmail_ + "\",\"sub\":\"" +
      serviceAccountEmail_ +
      "\",\"aud\":\"https://oauth2.googleapis.com/token\",\"iat\":" +
      String(static_cast<uint32_t>(now)) + ",\"exp\":" +
      String(static_cast<uint32_t>(now + kJwtLifetimeSeconds)) +
      ",\"target_audience\":\"" + cloudRunUrl_ + "\"}";
  return base64UrlEncode(reinterpret_cast<const uint8_t*>(json.c_str()),
                         json.length());
}

String CloudRunEventClient::buildNotificationBody(
    const String& message) const {
  const time_t now = time(nullptr);
  return String("{\"email\":\"") + AppConfig::EMAIL +
         "\",\"message\":\"" + message + "\",\"timestamp\":" +
         String(static_cast<uint32_t>(now)) + "}";
}

String CloudRunEventClient::base64UrlEncode(const uint8_t* data,
                                            size_t length) const {
  size_t encodedLength = 0;
  mbedtls_base64_encode(nullptr, 0, &encodedLength, data, length);

  String encoded;
  encoded.reserve(encodedLength + 1);
  char* buffer = new char[encodedLength + 1];
  if (buffer == nullptr) {
    return "";
  }

  if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(buffer),
                            encodedLength + 1, &encodedLength, data,
                            length) != 0) {
    delete[] buffer;
    return "";
  }

  buffer[encodedLength] = '\0';
  encoded = buffer;
  delete[] buffer;

  encoded.replace("+", "-");
  encoded.replace("/", "_");
  while (encoded.endsWith("=")) {
    encoded.remove(encoded.length() - 1);
  }

  return encoded;
}

bool CloudRunEventClient::signJwt(const String& signingInput,
                                  String& signatureBase64Url) const {
  unsigned char hash[32];
  mbedtls_md_context_t mdContext;
  mbedtls_md_init(&mdContext);

  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mdInfo == nullptr ||
      mbedtls_md_setup(&mdContext, mdInfo, 0) != 0 ||
      mbedtls_md_starts(&mdContext) != 0 ||
      mbedtls_md_update(
          &mdContext,
          reinterpret_cast<const unsigned char*>(signingInput.c_str()),
          signingInput.length()) != 0 ||
      mbedtls_md_finish(&mdContext, hash) != 0) {
    mbedtls_md_free(&mdContext);
    return false;
  }
  mbedtls_md_free(&mdContext);

  mbedtls_pk_context pkContext;
  mbedtls_pk_init(&pkContext);
  if (mbedtls_pk_parse_key(&pkContext,
                           reinterpret_cast<const unsigned char*>(
                               privateKeyPem_),
                           strlen(privateKeyPem_) + 1, nullptr, 0) != 0) {
    mbedtls_pk_free(&pkContext);
    return false;
  }

  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_context ctrDrbg;
  mbedtls_ctr_drbg_init(&ctrDrbg);

  const char* personalization = "cloud-run-jwt";
  if (mbedtls_ctr_drbg_seed(
          &ctrDrbg, mbedtls_entropy_func, &entropy,
          reinterpret_cast<const unsigned char*>(personalization),
          strlen(personalization)) != 0) {
    mbedtls_ctr_drbg_free(&ctrDrbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_pk_free(&pkContext);
    return false;
  }

  unsigned char signature[MBEDTLS_MPI_MAX_SIZE];
  size_t signatureLength = 0;
  const int signResult =
      mbedtls_pk_sign(&pkContext, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                      signature, &signatureLength, mbedtls_ctr_drbg_random,
                      &ctrDrbg);

  mbedtls_ctr_drbg_free(&ctrDrbg);
  mbedtls_entropy_free(&entropy);
  mbedtls_pk_free(&pkContext);

  if (signResult != 0) {
    return false;
  }

  signatureBase64Url = base64UrlEncode(signature, signatureLength);
  return !signatureBase64Url.isEmpty();
}

String CloudRunEventClient::extractJsonString(const String& json,
                                              const char* key) const {
  const String quotedKey = String("\"") + key + "\"";
  const int keyPos = json.indexOf(quotedKey);
  if (keyPos < 0) {
    return "";
  }

  const int colonPos = json.indexOf(':', keyPos + quotedKey.length());
  if (colonPos < 0) {
    return "";
  }

  const int startQuotePos = json.indexOf('"', colonPos + 1);
  if (startQuotePos < 0) {
    return "";
  }

  const int endQuotePos = json.indexOf('"', startQuotePos + 1);
  if (endQuotePos < 0) {
    return "";
  }

  return json.substring(startQuotePos + 1, endQuotePos);
}
