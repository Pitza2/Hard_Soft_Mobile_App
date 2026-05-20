#include <Arduino.h>
#include <Wire.h>

#include "components/BluetoothComponent.h"
#include "components/GyroComponent.h"
#include "components/TemperatureComponent.h"
#include "config/AppConfig.h"

namespace {
BluetoothComponent bluetooth;
GyroComponent gyro;
TemperatureComponent temperature;

constexpr uint32_t kGyroSampleIntervalMs = 20;

uint32_t lastSampleMs = 0;
uint32_t lastSendMs = 0;
bool gyroAvailable = false;
bool temperatureAvailable = false;
bool bluetoothAvailable = false;
bool fallAlertSent = false;

bool shouldSampleNow() {
  if (millis() - lastSampleMs < kGyroSampleIntervalMs) {
    return false;
  }

  lastSampleMs = millis();
  return true;
}

bool shouldSendNow() {
  if (millis() - lastSendMs < AppConfig::PUBLISH_INTERVAL_MS) {
    return false;
  }

  lastSendMs = millis();
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

bool buildSensorPayload(String& payload) {
  String gyroPayload;
  String temperaturePayload;
  const bool hasGyro = readGyroPayload(gyroPayload);
  const bool hasTemperature = readTemperaturePayload(temperaturePayload);

  if (!hasGyro && !hasTemperature) {
    return false;
  }

  payload = "{";
  bool hasField = false;

  if (hasGyro) {
    payload += "\"gyro\":";
    payload += gyroPayload;
    hasField = true;
  }

  if (hasTemperature) {
    if (hasField) {
      payload += ",";
    }
    payload += "\"body_temp\":";
    payload += temperaturePayload;
  }

  payload += "}";
  return true;
}

bool buildBleAlertPayload(String& payload) {
  payload = "{\"data\":{";

  String temperaturePayload;
  const bool hasTemperature = readTemperaturePayload(temperaturePayload);
  if (hasTemperature) {
    payload += "\"body_temp\":";
    payload += temperaturePayload;
    payload += ",";
  }

  payload += "\"alert\":{\"message\":\"Fall down detected\"}}}";
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);

  bluetoothAvailable = bluetooth.begin();
  if (!bluetoothAvailable) {
    Serial.println("Bluetooth disabled");
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
  if (bluetoothAvailable) {
    bluetooth.loop();
  }

  if (shouldSampleNow()) {
    sampleGyro();
  }

  if (!shouldSendNow()) {
    delay(2);
    return;
  }

  String payload;
  if (!buildSensorPayload(payload)) {
    return;
  }

  Serial.print("Sensor data: ");
  Serial.println(payload);

  if (!gyroAvailable) {
    return;
  }

  const bool fallDetected = gyro.isFallDetected();
  if (!fallDetected) {
    fallAlertSent = false;
    return;
  }

  if (fallAlertSent || !bluetoothAvailable) {
    return;
  }

  String alertPayload;
  buildBleAlertPayload(alertPayload);
  bluetooth.sendMessage(alertPayload);
  Serial.println("BLE fall alert sent");
  fallAlertSent = true;
}
