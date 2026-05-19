#include "core/FirebaseClient.h"

#include <WiFi.h>
#include <time.h>

#include "config/AppConfig.h"

namespace {
constexpr time_t kValidEpochThreshold = 1700000000;
constexpr uint32_t kAuthReadyTimeoutMs = 10000;
}

AppFirebaseClient::AppFirebaseClient(const char* wifiSsid,
                                     const char* wifiPassword,
                                     const char* databaseUrl,
                                     const char* authToken)
    : wifiSsid_(wifiSsid),
      wifiPassword_(wifiPassword),
      databaseUrl_(databaseUrl),
      authToken_(authToken),
      userAuth_(AppConfig::FIREBASE_API_KEY, AppConfig::FIREBASE_USER_EMAIL,
                AppConfig::FIREBASE_USER_PASSWORD),
      asyncClient_(ssl_) {}

bool AppFirebaseClient::begin() {
  if (!ensureWifiConnected() || !ensureTimeSynced()) {
    return false;
  }

  if (initialized_) {
    return ensureReady();
  }

  ssl_.setInsecure();
  initializeApp(asyncClient_, app_, getAuth(userAuth_));
  app_.getApp<RealtimeDatabase>(database_);
  database_.url(databaseUrl_);
  initialized_ = true;

  Serial.println("Firebase initialized");
  return ensureReady();
}

bool AppFirebaseClient::publish(const char* componentName,
                                const String& jsonPayload) {
  if (!ensureReady()) {
    return false;
  }

  return database_.set<object_t>(asyncClient_, buildPath(componentName),
                                 object_t(jsonPayload.c_str()));
}

void AppFirebaseClient::loop() {
  if (initialized_) {
    app_.loop();
  }
}

bool AppFirebaseClient::ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.printf("Connecting to WiFi SSID %s\n", wifiSsid_);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid_, wifiPassword_);

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startMs < AppConfig::WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection timed out");
    return false;
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool AppFirebaseClient::ensureTimeSynced() {
  if (time(nullptr) > kValidEpochThreshold) {
    return true;
  }

  configTime(0, 0, AppConfig::NTP_SERVER);
  const uint32_t startMs = millis();
  while (time(nullptr) <= kValidEpochThreshold &&
         millis() - startMs < AppConfig::NTP_SYNC_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (time(nullptr) <= kValidEpochThreshold) {
    Serial.println("NTP sync failed");
    return false;
  }

  return true;
}

bool AppFirebaseClient::ensureReady() {
  if (!initialized_ && !begin()) {
    return false;
  }

  if (!ensureWifiConnected() || !ensureTimeSynced()) {
    return false;
  }

  const uint32_t startMs = millis();
  while (!app_.ready() && millis() - startMs < kAuthReadyTimeoutMs) {
    app_.loop();
    delay(10);
  }

  if (!app_.ready()) {
    Serial.println("Firebase auth not ready");
    return false;
  }

  return true;
}

const String& AppFirebaseClient::getDeviceId() const {
  if (!deviceId_.isEmpty()) {
    return deviceId_;
  }

  deviceId_ = WiFi.macAddress();
  deviceId_.replace(":", "-");
  deviceId_.toLowerCase();
  return deviceId_;
}

String AppFirebaseClient::buildPath(const char* componentName) const {
  String path("/devices/");
  path += getDeviceId();
  path += "/";
  path += componentName;
  return path;
}
