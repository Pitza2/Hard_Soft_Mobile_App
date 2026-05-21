#include "components/BuzzerComponent.h"

#include "config/AppConfig.h"

const char* BuzzerComponent::name() const { return "buzzer"; }

bool BuzzerComponent::begin() {
  pinMode(AppConfig::STOP_BUTTON_PIN, INPUT_PULLUP);
  ledcSetup(AppConfig::BUZZER_CHANNEL1, AppConfig::BUZZER_BASE_FREQUENCY_HZ,
            AppConfig::BUZZER_PWM_RESOLUTION);
  ledcSetup(AppConfig::BUZZER_CHANNEL2, AppConfig::BUZZER_BASE_FREQUENCY_HZ,
            AppConfig::BUZZER_PWM_RESOLUTION);
  ledcAttachPin(AppConfig::BUZZER_PIN1, AppConfig::BUZZER_CHANNEL1);
  ledcAttachPin(AppConfig::BUZZER_PIN2, AppConfig::BUZZER_CHANNEL2);
  stop();
  return true;
}

bool BuzzerComponent::read(String& jsonPayload) {
  jsonPayload = String("{\"active\":") + (active_ ? "true" : "false") +
                ",\"frequency_hz\":" + String(frequencyHz_) + "}";
  return true;
}

void BuzzerComponent::loop() {
  if (active_ && wasStopButtonPressed()) {
    stop();
    Serial.println("Fall alarm stopped");
    return;
  }

  if (!active_) {
    return;
  }

  const uint32_t nowMs = millis();
  const uint32_t stepIntervalMs =
      phase_ < 2 ? AppConfig::BUZZER_SWEEP_STEP_MS
                 : (phase_ % 2 == 0 ? AppConfig::BUZZER_BEEP_ON_MS
                                    : AppConfig::BUZZER_BEEP_OFF_MS);
  if (nowMs - lastStepAtMs_ < stepIntervalMs) {
    return;
  }

  lastStepAtMs_ = nowMs;

  if (phase_ == 0) {
    frequencyHz_ += AppConfig::BUZZER_FREQUENCY_STEP_HZ;
    if (frequencyHz_ >= AppConfig::BUZZER_MAX_FREQUENCY_HZ) {
      frequencyHz_ = AppConfig::BUZZER_MAX_FREQUENCY_HZ;
      phase_ = 1;
    }
    playBoth(frequencyHz_);
    return;
  }

  if (phase_ == 1) {
    frequencyHz_ -= AppConfig::BUZZER_FREQUENCY_STEP_HZ;
    if (frequencyHz_ <= AppConfig::BUZZER_BASE_FREQUENCY_HZ) {
      frequencyHz_ = AppConfig::BUZZER_BASE_FREQUENCY_HZ;
      phase_ = 2;
      beepCount_ = 0;
    }
    playBoth(frequencyHz_);
    return;
  }

  if (phase_ % 2 == 0) {
    playBoth(AppConfig::BUZZER_BEEP_FREQUENCY_HZ);
  } else {
    stopBoth();
    ++beepCount_;
    if (beepCount_ >= AppConfig::BUZZER_BEEP_COUNT) {
      phase_ = 0;
      frequencyHz_ = AppConfig::BUZZER_BASE_FREQUENCY_HZ;
      beepCount_ = 0;
      return;
    }
  }

  ++phase_;
}

void BuzzerComponent::startAlarm() {
  active_ = true;
  lastStepAtMs_ = millis();
  phase_ = 0;
  frequencyHz_ = AppConfig::BUZZER_BASE_FREQUENCY_HZ;
  beepCount_ = 0;
  playBoth(frequencyHz_);
  Serial.println("Fall alarm started");
}

void BuzzerComponent::stop() {
  active_ = false;
  phase_ = 0;
  frequencyHz_ = 0;
  beepCount_ = 0;
  stopBoth();
}

bool BuzzerComponent::isActive() const { return active_; }

void BuzzerComponent::playBoth(int frequency) {
  frequencyHz_ = frequency;
  ledcWriteTone(AppConfig::BUZZER_CHANNEL1, frequency);
  ledcWriteTone(AppConfig::BUZZER_CHANNEL2, frequency);
  ledcWrite(AppConfig::BUZZER_CHANNEL1, AppConfig::BUZZER_DUTY_CYCLE);
  ledcWrite(AppConfig::BUZZER_CHANNEL2, AppConfig::BUZZER_DUTY_CYCLE);
}

void BuzzerComponent::stopBoth() {
  ledcWriteTone(AppConfig::BUZZER_CHANNEL1, 0);
  ledcWriteTone(AppConfig::BUZZER_CHANNEL2, 0);
  ledcWrite(AppConfig::BUZZER_CHANNEL1, 0);
  ledcWrite(AppConfig::BUZZER_CHANNEL2, 0);
}

bool BuzzerComponent::wasStopButtonPressed() {
  const bool reading = digitalRead(AppConfig::STOP_BUTTON_PIN);

  if (reading != stopButtonLastReading_) {
    stopButtonLastDebounceMs_ = millis();
    stopButtonLastReading_ = reading;
  }

  if (millis() - stopButtonLastDebounceMs_ > AppConfig::BUTTON_DEBOUNCE_MS &&
      reading != stopButtonStableState_) {
    stopButtonStableState_ = reading;
    if (stopButtonStableState_ == LOW) {
      return true;
    }
  }

  return false;
}
