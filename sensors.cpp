#include <Arduino.h>
#include <Wire.h>

#include "components/BuzzerComponent.h"
#include "components/GyroComponent.h"
#include "components/PulseOximeterComponent.h"
#include "components/TemperatureComponent.h"
#include "config/AppConfig.h"
#include "core/FirebaseClient.h"

FirebaseClient firebase(AppConfig::WIFI_SSID,
                        AppConfig::WIFI_PASSWORD,
                        AppConfig::FIREBASE_DATABASE_URL,
                        AppConfig::FIREBASE_AUTH_TOKEN);
BuzzerComponent buzzer;
GyroComponent gyro;
PulseOximeterComponent pulseOximeter;
TemperatureComponent temperature;

uint32_t lastPublishMs = 0;
bool hadValidOxygenReading = false;
bool gyroAvailable = false;
bool pulseOximeterAvailable = false;
bool temperatureAvailable = false;

bool shouldPublishNow() {
  if (millis() - lastPublishMs < AppConfig::PUBLISH_INTERVAL_MS) {
    return false;
  }

  lastPublishMs = millis();
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

bool readTemperaturePayload(String& temperaturePayload) {
  if (!temperatureAvailable) {
    delay(250);
    return false;
  }

  if (!temperature.read(temperaturePayload)) {
    Serial.println("Temperature read failed");
    return false;
  }

  return true;
}

bool readPulsePayload(String& pulsePayload) {
  if (!pulseOximeterAvailable) {
    return false;
  }

  if (!pulseOximeter.read(pulsePayload)) {
    Serial.println("Pulse oximeter read failed");
    return false;
  }

  const bool hasValidOxygenReading = pulseOximeter.hasValidOxygenReading();
  if (hasValidOxygenReading && !hadValidOxygenReading) {
    buzzer.beepFor(AppConfig::BUZZER_ALERT_DURATION_MS);
  }

  hadValidOxygenReading = hasValidOxygenReading;
  return true;
}

String buildSensorsPayload(const String& gyroPayload,
                           const String& temperaturePayload,
                           const String& pulsePayload,
                           bool includePulsePayload) {
  String payload = String("{\"gyro\":") + gyroPayload +
                   ",\"body_temp\":" + temperaturePayload;
  if (includePulsePayload) {
    payload += ",\"pulse_oximeter\":";
    payload += pulsePayload;
  }
  payload += "}";
  return payload;
}

void publishSensorsData(const String& payload) {
  if (!firebase.publish("sensors", payload)) {
    Serial.println("Publish failed for sensors");
    return;
  }

  Serial.print("Sensors data: ");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);

  firebase.begin();
  buzzer.begin();
  gyroAvailable = gyro.begin();
  pulseOximeterAvailable = pulseOximeter.begin();
  temperatureAvailable = temperature.begin();

  if (!gyroAvailable) {
    Serial.println("Gyro disabled");
  }
  if (!pulseOximeterAvailable) {
    Serial.println("Pulse oximeter disabled");
  }
  if (!temperatureAvailable) {
    Serial.println("Temperature sensor disabled");
  }
}

void loop() {
  if (gyroAvailable && !gyro.update()) {
    Serial.println("Gyro read failed");
  }

  if (!shouldPublishNow()) {
    delay(10);
    return;
  }

  String gyroPayload = "null";
  if (gyroAvailable && !readGyroPayload(gyroPayload)) {
    return;
  }

  String temperaturePayload;
  if (!readTemperaturePayload(temperaturePayload)) {
    return;
  }

  if (!pulseOximeterAvailable) {
    publishSensorsData(
        buildSensorsPayload(gyroPayload, temperaturePayload, "", false));
    return;
  }

  String pulsePayload;
  if (!readPulsePayload(pulsePayload)) {
    return;
  }

  publishSensorsData(
      buildSensorsPayload(gyroPayload, temperaturePayload, pulsePayload, true));
}
