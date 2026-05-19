#pragma once

#include <Arduino.h>

class CloudRunEventClient {
 public:
  CloudRunEventClient(const char* serviceAccountEmail, const char* privateKeyId,
                      const char* privateKeyPem, const char* cloudRunUrl);

  bool begin();
  bool sendNotification(const String& message);

 private:
  bool ensureTimeSynced();
  bool hasValidConfig() const;
  bool ensureIdToken(String& idToken);
  bool createSignedJwt(String& signedJwt) const;
  bool exchangeJwtForIdToken(const String& signedJwt, String& idToken);
  bool callCloudRun(const String& idToken, const String& body);
  String buildJwtHeader() const;
  String buildJwtPayload(time_t now) const;
  String buildNotificationBody(const String& message) const;
  String base64UrlEncode(const uint8_t* data, size_t length) const;
  bool signJwt(const String& signingInput, String& signatureBase64Url) const;
  String extractJsonString(const String& json, const char* key) const;

  const char* serviceAccountEmail_;
  const char* privateKeyId_;
  const char* privateKeyPem_;
  const char* cloudRunUrl_;
  String cachedIdToken_;
  time_t idTokenExpiresAt_ = 0;
  bool timeSynced_ = false;
};
