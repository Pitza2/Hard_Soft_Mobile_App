#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

#define ENABLE_DATABASE
#define ENABLE_USER_AUTH
#include <FirebaseClient.h>

class AppFirebaseClient {
 public:
  AppFirebaseClient(const char* wifiSsid, const char* wifiPassword,
                    const char* databaseUrl, const char* authToken);

  bool begin();
  bool publish(const char* componentName, const String& jsonPayload);
  void loop();

 private:
  bool ensureWifiConnected();
  bool ensureTimeSynced();
  bool ensureReady();
  const String& getDeviceId() const;
  String buildPath(const char* componentName) const;

  const char* wifiSsid_;
  const char* wifiPassword_;
  const char* databaseUrl_;
  const char* authToken_;
  WiFiClientSecure ssl_;
  UserAuth userAuth_;
  FirebaseApp app_;
  RealtimeDatabase database_;
  AsyncClientClass asyncClient_;
  bool initialized_ = false;
  mutable String deviceId_;
};
