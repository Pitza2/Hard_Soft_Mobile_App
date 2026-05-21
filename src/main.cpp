#include <Arduino.h>
#include <Wire.h>

#include <math.h>

#include "components/BluetoothComponent.h"
#include "components/GyroComponent.h"
#include "components/PulseOximeterComponent.h"
#include "components/SetupDisplayComponent.h"
#include "components/TemperatureComponent.h"
#include "config/AppConfig.h"

namespace {
BluetoothComponent bluetooth;
GyroComponent gyro;
PulseOximeterComponent pulseOximeter;
SetupDisplayComponent setupDisplay;
TemperatureComponent temperature;

constexpr int kGyroInterruptPin = 19;
constexpr uint32_t kSensorSendIntervalMs = 1000;  // 1 sample per second
constexpr uint32_t kGyroDebugPrintEverySamples = 20;

uint32_t lastSensorSendMs = 0;
uint32_t gyroSampleCounter = 0;
bool gyroAvailable = false;
bool pulseAvailable = false;
bool temperatureAvailable = false;

bool shouldSendSensorPacket() {
  const uint32_t nowMs = millis();
  if (nowMs - lastSensorSendMs < kSensorSendIntervalMs) {
    return false;
  }

  lastSensorSendMs = nowMs;
  return true;
}

void sampleSlowSensors() {
  String ignoredPayload;

  if (pulseAvailable && !pulseOximeter.read(ignoredPayload)) {
    Serial.println("Pulse oximeter read failed");
  }

  if (temperatureAvailable && !temperature.read(ignoredPayload)) {
    Serial.println("Temperature read failed");
  }
}

String buildSensorPacket() {
  const int fall = gyroAvailable && gyro.isFallDetected() ? 1 : 0;
  const int heartRate = pulseAvailable ? pulseOximeter.heartRate() : 22;
  const int oxygen = pulseAvailable ? pulseOximeter.oxygen() : 33;
  const uint32_t steps = gyroAvailable ? gyro.stepCount() : 44;
  const float tempC =
      temperatureAvailable && !isnan(temperature.temperatureC())
          ? temperature.temperatureC()
          : 55.0f;

  return String("{\"action\":\"sensData\",\"fall\":") + String(fall) +
         ",\"hr\":" + String(heartRate) + ",\"spo2\":" + String(oxygen) +
         ",\"steps\":" + String(steps) + ",\"temp\":" + String(tempC, 1) + "}";
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);

  bluetooth.begin();

  gyro.setInterruptPin(kGyroInterruptPin);
  gyroAvailable = gyro.begin();
  if (!gyroAvailable) {
    Serial.println("Gyro init failed");
  }

  pulseAvailable = pulseOximeter.begin();
  if (!pulseAvailable) {
    Serial.println("Pulse oximeter init failed");
  }

  temperatureAvailable = temperature.begin();
  if (!temperatureAvailable) {
    Serial.println("Temperature init failed");
  }

  setupDisplay.setBluetoothComponent(&bluetooth);
  setupDisplay.setGyroComponent(&gyro);
  if (!setupDisplay.begin()) {
    Serial.println("Setup display init failed");
  }

  Serial.println("Sensor BLE streaming started");
}

void loop() {
  bluetooth.loop();
  setupDisplay.loop();

  if (gyroAvailable) {
    if (gyro.sampleIfNeeded()) {
      ++gyroSampleCounter;
      if (gyroSampleCounter % kGyroDebugPrintEverySamples == 0) {
        String gyroPayload;
        if (gyro.read(gyroPayload)) {
          Serial.print("Gyro sample ");
          Serial.print(gyroSampleCounter);
          Serial.print(": ");
          Serial.println(gyroPayload);
        }
      }
    }
  }

  if (!shouldSendSensorPacket()) {
    delay(1);
    return;
  }

  sampleSlowSensors();

  const String payload = buildSensorPacket();
  bluetooth.sendMessage(payload);
  Serial.println(payload);
}
