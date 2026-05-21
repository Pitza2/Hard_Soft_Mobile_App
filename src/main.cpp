#include <Arduino.h>
#include <Wire.h>

#include <math.h>

#include "components/BluetoothComponent.h"
#include "components/BuzzerComponent.h"
#include "components/GyroComponent.h"
#include "components/PulseOximeterComponent.h"
#include "components/SetupDisplayComponent.h"
#include "components/TemperatureComponent.h"
#include "config/AppConfig.h"

namespace {
constexpr int kAlertLedPin = 32;
constexpr int kGreenLedPin = 33;
BluetoothComponent bluetooth;
BuzzerComponent buzzer;
GyroComponent gyro;
PulseOximeterComponent pulseOximeter;
SetupDisplayComponent setupDisplay;
TemperatureComponent temperature;

constexpr uint32_t kSensorSendIntervalMs = 1000;  // 1 sample per second
constexpr uint32_t kGyroDebugPrintEverySamples = 20;
constexpr uint32_t kFallLedBlinkIntervalMs = 150;

uint32_t lastSensorSendMs = 0;
uint32_t lastGyroPollMs = 0;
uint32_t gyroSampleCounter = 0;
bool gyroAvailable = false;
bool pulseAvailable = false;
bool temperatureAvailable = false;
bool fallAlarmArmed = true;
bool fallDismissedForCurrentEvent = false;
bool lastRawFallDetected = false;
bool alertButtonLastReading = HIGH;
bool alertButtonStableState = HIGH;
uint32_t alertButtonLastDebounceMs = 0;
uint32_t lastLedBlinkMs = 0;
bool ledBlinkState = false;

bool shouldSendSensorPacket() {
  const uint32_t nowMs = millis();
  if (nowMs - lastSensorSendMs < kSensorSendIntervalMs) {
    return false;
  }

  lastSensorSendMs = nowMs;
  return true;
}

bool isConfigurationComplete() {
  return setupDisplay.isConfigurationComplete();
}

bool isRawFallDetectedForApp() {
  return gyroAvailable && gyro.isFallDetected();
}

bool isFallDetectedForApp() {
  return isRawFallDetectedForApp() && !fallDismissedForCurrentEvent;
}

void beginInfrastructure() {
  Serial.begin(115200);
  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);
  pinMode(AppConfig::ALERT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(kAlertLedPin, OUTPUT);
  pinMode(kGreenLedPin, OUTPUT);
}

void beginComponents() {
  bluetooth.begin();
  buzzer.begin();

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
}

void beginDisplay() {
  setupDisplay.setBluetoothComponent(&bluetooth);
  setupDisplay.setGyroComponent(&gyro);
  if (!setupDisplay.begin()) {
    Serial.println("Setup display init failed");
  }
}

bool wasAlertButtonPressed() {
  const bool reading = digitalRead(AppConfig::ALERT_BUTTON_PIN);

  if (reading != alertButtonLastReading) {
    alertButtonLastDebounceMs = millis();
    alertButtonLastReading = reading;
  }

  if (millis() - alertButtonLastDebounceMs > AppConfig::BUTTON_DEBOUNCE_MS &&
      reading != alertButtonStableState) {
    alertButtonStableState = reading;
    if (alertButtonStableState == LOW) {
      return true;
    }
  }

  return false;
}

void sampleSlowSensors() {
  String ignoredPayload;

  if (pulseAvailable && !pulseOximeter.read(ignoredPayload)) {
    Serial.println("Pulse oximeter read failed");
  }

  if (temperatureAvailable && !temperature.read(ignoredPayload)) {
    Serial.println("Temperature read failed");
  }

  const int heartRate = pulseAvailable ? pulseOximeter.heartRate() : 72;
  const int oxygen = pulseAvailable ? pulseOximeter.oxygen() : 98;
  const float tempC =
      temperatureAvailable && !isnan(temperature.temperatureC())
          ? temperature.temperatureC()
          : 36.7f;
  setupDisplay.setVitals(heartRate, oxygen, tempC);
}

String buildSensorPacket() {
  const int fall = isFallDetectedForApp() ? 1 : 0;
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

void sendDismissFallEventIfNeeded() {
  if (!buzzer.consumeStoppedByUserEvent()) {
    return;
  }

  fallDismissedForCurrentEvent = true;
  const String payload = "{\"action\":\"dismiss_fall\"}";
  bluetooth.sendMessage(payload);
  Serial.println(payload);
}

void handleFallAlarm() {
  const bool rawFallDetected = isRawFallDetectedForApp();
  if (!rawFallDetected) {
    fallDismissedForCurrentEvent = false;
    fallAlarmArmed = true;
    lastRawFallDetected = false;
    return;
  }

  if (!lastRawFallDetected) {
    fallDismissedForCurrentEvent = false;
    fallAlarmArmed = true;
    const String payload = buildSensorPacket();
    bluetooth.sendMessage(payload);
    Serial.println(payload);
  }
  lastRawFallDetected = true;

  if (fallDismissedForCurrentEvent) {
    return;
  }

  if (fallAlarmArmed && !buzzer.isActive()) {
    buzzer.startAlarm();
    fallAlarmArmed = false;
  }
}

void maybePrintGyroSample() {
  if (!isFallDetectedForApp()) {
    return;
  }

  ++gyroSampleCounter;
  if (gyroSampleCounter % kGyroDebugPrintEverySamples != 0) {
    return;
  }

  String gyroPayload;
  if (!gyro.read(gyroPayload)) {
    return;
  }

  Serial.print("Gyro sample ");
  Serial.print(gyroSampleCounter);
  Serial.print(": ");
  Serial.println(gyroPayload);
}

void processGyroSample() {
  const uint32_t nowMs = millis();
  if (!gyroAvailable ||
      nowMs - lastGyroPollMs < AppConfig::GYRO_POLL_INTERVAL_MS) {
    return;
  }

  lastGyroPollMs = nowMs;
  if (!gyro.update()) {
    return;
  }

  handleFallAlarm();
  maybePrintGyroSample();
}

void sendSensorPacket() {
  sampleSlowSensors();

  const String payload = buildSensorPacket();
  bluetooth.sendMessage(payload);
  Serial.println(payload);
}

void processManualFallTrigger() {
  if (!gyroAvailable) {
    return;
  }

  if (!wasAlertButtonPressed()) {
    return;
  }

  gyro.triggerFallDetected();
}

void updateStatusLeds() {
  const bool fallDetected = isFallDetectedForApp();
  if (!fallDetected) {
    digitalWrite(kAlertLedPin, LOW);
    digitalWrite(kGreenLedPin, HIGH);
    ledBlinkState = false;
    lastLedBlinkMs = millis();
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastLedBlinkMs >= kFallLedBlinkIntervalMs) {
    lastLedBlinkMs = nowMs;
    ledBlinkState = !ledBlinkState;
  }

  digitalWrite(kAlertLedPin, ledBlinkState ? HIGH : LOW);
  digitalWrite(kGreenLedPin, ledBlinkState ? HIGH : LOW);
}
}  // namespace

void setup() {
  beginInfrastructure();
  beginComponents();
  beginDisplay();
  updateStatusLeds();

  Serial.println("Setup completed");
}

void loop() {
  bluetooth.loop();
  buzzer.loop();
  sendDismissFallEventIfNeeded();
  setupDisplay.setInputEnabled(!buzzer.isActive());
  setupDisplay.loop();
  processManualFallTrigger();
  processGyroSample();
  updateStatusLeds();

  if (!isConfigurationComplete()) {
    delay(1);
    return;
  }

  if (!shouldSendSensorPacket()) {
    delay(1);
    return;
  }

  sendSensorPacket();
}
