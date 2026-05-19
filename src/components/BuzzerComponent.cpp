#include "components/BuzzerComponent.h"

#include "config/AppConfig.h"

const char* BuzzerComponent::name() const { return "buzzer"; }

bool BuzzerComponent::begin() {
  pinMode(AppConfig::BUZZER_PIN, OUTPUT);
  digitalWrite(AppConfig::BUZZER_PIN, LOW);
  buzzerOn_ = false;
  toneStarted_ = false;
  return true;
}

bool BuzzerComponent::read(String& jsonPayload) {
  buzzerOn_ = !buzzerOn_;

  if (buzzerOn_) {
    tone(AppConfig::BUZZER_PIN, AppConfig::BUZZER_FREQUENCY_HZ);
  } else {
    noTone(AppConfig::BUZZER_PIN);
  }

  jsonPayload = String("{\"pin\":") + String(AppConfig::BUZZER_PIN) +
                ",\"frequency_hz\":" + String(AppConfig::BUZZER_FREQUENCY_HZ) +
                ",\"active\":" + String(buzzerOn_ ? "true" : "false") + "}";
  return true;
}

void BuzzerComponent::beepFor(uint32_t durationMs) {
  tone(AppConfig::BUZZER_PIN, AppConfig::BUZZER_FREQUENCY_HZ, durationMs);
  buzzerOn_ = true;
  toneStarted_ = true;
}

void BuzzerComponent::stop() {
  if (toneStarted_) {
    noTone(AppConfig::BUZZER_PIN);
    toneStarted_ = false;
  }
  buzzerOn_ = false;
}
