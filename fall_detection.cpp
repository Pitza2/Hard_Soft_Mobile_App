#include <Arduino.h>
#include <Wire.h>

#include "components/GyroComponent.h"
#include "components/TemperatureComponent.h"
#include "config/AppConfig.h"
#include "core/FirebaseClient.h"

namespace {
AppFirebaseClient firebase(AppConfig::WIFI_SSID,
                           AppConfig::WIFI_PASSWORD,
                           AppConfig::FIREBASE_DATABASE_URL,
                           AppConfig::FIREBASE_AUTH_TOKEN);
GyroComponent gyro;
TemperatureComponent temperature;

constexpr uint32_t kGyroSampleIntervalMs = 20;

uint32_t lastSampleMs = 0;
uint32_t lastPublishMs = 0;
bool gyroAvailable = false;
bool temperatureAvailable = false;

bool shouldSampleNow() {
  if (millis() - lastSampleMs < kGyroSampleIntervalMs) {
    return false;
  }

  lastSampleMs = millis();
  return true;
}

bool shouldPublishNow() {
  if (millis() - lastPublishMs < AppConfig::PUBLISH_INTERVAL_MS) {
    return false;
  }

  lastPublishMs = millis();
  return true;
}

bool sampleGyro() {
  if (!gyroAvailable) {
    return false;
  }

  if (!gyro.update()) {
    Serial.println("Gyro sample failed");
    return false;
  }

  return true;
}

bool readGyroPayload(String& gyroPayload) {
  if (!gyroAvailable) {
    return false;
  }

  if (!gyro.read(gyroPayload)) {
    Serial.println("Gyro payload build failed");
    return false;
  }

  return true;
}

void publishGyroData(const String& payload) {
  if (!firebase.publish("gyro", payload)) {
    Serial.println("Publish failed for gyro");
    return;
  }

  Serial.print("Gyro data: ");
  Serial.println(payload);
}

bool readTemperaturePayload(String& temperaturePayload) {
  if (!temperatureAvailable) {
    return false;
  }

  if (!temperature.read(temperaturePayload)) {
    Serial.println("Temperature payload build failed");
    return false;
  }

  return true;
}

void publishTemperatureData(const String& payload) {
  if (!firebase.publish("body_temp", payload)) {
    Serial.println("Publish failed for body_temp");
    return;
  }

  Serial.print("Temperature data: ");
  Serial.println(payload);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);

  if (!firebase.begin()) {
    Serial.println("Firebase init failed");
  }

  gyroAvailable = gyro.begin();
  if (!gyroAvailable) {
    Serial.println("Gyro disabled");
  }

  temperatureAvailable = temperature.begin();
  if (!temperatureAvailable) {
    Serial.println("Temperature sensor disabled");
  }
}

void loop() {
  firebase.loop();

  if (shouldSampleNow()) {
    sampleGyro();
  }

  if (!shouldPublishNow()) {
    delay(2);
    return;
  }

  String gyroPayload;
  if (readGyroPayload(gyroPayload)) {
    publishGyroData(gyroPayload);
  }

  String temperaturePayload;
  if (readTemperaturePayload(temperaturePayload)) {
    publishTemperatureData(temperaturePayload);
  }
}
