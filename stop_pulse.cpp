#include <Arduino.h>
#include <Wire.h>

#include "components/PulseOximeterComponent.h"
#include "config/AppConfig.h"
#include "core/FirebaseClient.h"

namespace {
AppFirebaseClient firebase(AppConfig::WIFI_SSID,
                           AppConfig::WIFI_PASSWORD,
                           AppConfig::FIREBASE_DATABASE_URL,
                           AppConfig::FIREBASE_AUTH_TOKEN);
PulseOximeterComponent pulseOximeter;

constexpr uint32_t kPulseRestartDelayMs = 120000;
constexpr uint32_t kRequiredValidSamples = 20;

uint32_t lastPublishMs = 0;
uint32_t pulseStateStartedMs = 0;
uint32_t validPulseSampleCount = 0;
bool pulseOximeterAvailable = false;
bool pulseSleeping = false;

bool shouldPublishNow() {
  if (millis() - lastPublishMs < AppConfig::PUBLISH_INTERVAL_MS) {
    return false;
  }

  lastPublishMs = millis();
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

  return true;
}

void publishPulseData(const String& payload) {
  if (!firebase.publish("pulse_oximeter", payload)) {
    Serial.println("Publish failed for pulse_oximeter");
    return;
  }

  Serial.print("Pulse oximeter data: ");
  Serial.println(payload);
}
}  // namespace

void setPulseSleeping(bool shouldSleep) {
  if (!pulseOximeterAvailable || pulseSleeping == shouldSleep) {
    return;
  }

  const bool success =
      shouldSleep ? pulseOximeter.sleep() : pulseOximeter.wake();
  if (!success) {
    return;
  }

  pulseSleeping = shouldSleep;
  pulseStateStartedMs = millis();
  if (!shouldSleep) {
    validPulseSampleCount = 0;
  }
  Serial.println(shouldSleep ? "Pulse oximeter sleeping"
                             : "Pulse oximeter awake");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Wire.setClock(100000);

  if (!firebase.begin()) {
    Serial.println("Firebase init failed");
  }

  pulseOximeterAvailable = pulseOximeter.begin();
  if (!pulseOximeterAvailable) {
    Serial.println("Pulse oximeter disabled");
  } else {
    pulseStateStartedMs = millis();
  }
}

void loop() {
  firebase.loop();

  if (pulseSleeping) {
    if (millis() - pulseStateStartedMs >= kPulseRestartDelayMs) {
      setPulseSleeping(false);
    } else {
      delay(10);
      return;
    }
  }

  if (!shouldPublishNow()) {
    delay(10);
    return;
  }

  String pulsePayload;
  if (!readPulsePayload(pulsePayload)) {
    return;
  }

  publishPulseData(pulsePayload);

  if (!pulseOximeter.hasValidOxygenReading()) {
    return;
  }

  ++validPulseSampleCount;
  Serial.printf("Valid pulse sample count: %lu/%lu\n", validPulseSampleCount,
                kRequiredValidSamples);
  if (validPulseSampleCount >= kRequiredValidSamples) {
    setPulseSleeping(true);
  }
}
