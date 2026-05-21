#include "components/SetupDisplayComponent.h"

#include <math.h>
#include <Wire.h>

#include "components/BluetoothComponent.h"
#include "components/GyroComponent.h"

namespace {
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kOledResetPin = -1;
constexpr uint8_t kOledAddress = 0x3D;
constexpr int kI2cSdaPin = 21;
constexpr int kI2cSclPin = 22;
constexpr int kReadyButtonPin = 18;
constexpr uint32_t kWelcomeDurationMs = 3500;
constexpr uint32_t kSecondPromptDelayMs = 5000;
constexpr char kPreferencesNamespace[] = "setup";
constexpr char kPrefDoneKey[] = "done";
constexpr char kPrefStandKey[] = "stand";
constexpr char kPrefSeatKey[] = "seat";
constexpr char kPrefWalkKey[] = "walk";
constexpr char kPrefMeanKey[] = "mean";
}

SetupDisplayComponent::SetupDisplayComponent()
    : display_(kScreenWidth, kScreenHeight, &Wire, kOledResetPin) {}

const char* SetupDisplayComponent::name() const { return "setup_display"; }

bool SetupDisplayComponent::begin() {
  pinMode(kReadyButtonPin, INPUT_PULLUP);
  Wire.begin(kI2cSdaPin, kI2cSclPin);

  if (!display_.begin(SSD1306_SWITCHCAPVCC, kOledAddress)) {
    Serial.println("OLED init failed");
    return false;
  }

  calibrationLoaded_ = loadCalibration();
  if (calibrationLoaded_ && gyro_ != nullptr) {
    gyro_->setCalibratedPostureMean(overallMeanAngleDeg_);
  }
  if (calibrationLoaded_) {
    enterState(UiState::kDone);
    watchStartedAtMs_ = millis();
    doneScreen_ = DoneScreen::kWatch;
    drawWatchScreen();
  } else {
    enterState(UiState::kWelcome);
    drawWelcomeFrame();
  }
  initialized_ = true;
  return true;
}

bool SetupDisplayComponent::read(String& jsonPayload) {
  jsonPayload = String("{\"state\":\"") + stateName() +
                "\",\"calibrated\":" +
                (calibrationLoaded_ ? "true" : "false") +
                ",\"standing_mean_deg\":" +
                (isnan(standingMeanAngleDeg_) ? String("null")
                                              : String(standingMeanAngleDeg_, 2)) +
                ",\"seated_mean_deg\":" +
                (isnan(seatedMeanAngleDeg_) ? String("null")
                                            : String(seatedMeanAngleDeg_, 2)) +
                ",\"walking_mean_deg\":" +
                (isnan(walkingMeanAngleDeg_) ? String("null")
                                             : String(walkingMeanAngleDeg_, 2)) +
                ",\"overall_mean_deg\":" +
                (isnan(overallMeanAngleDeg_) ? String("null")
                                             : String(overallMeanAngleDeg_, 2)) +
                "}";
  return true;
}

void SetupDisplayComponent::setBluetoothComponent(
    const BluetoothComponent* bluetooth) {
  bluetooth_ = bluetooth;
}

void SetupDisplayComponent::setGyroComponent(GyroComponent* gyro) {
  gyro_ = gyro;
}

void SetupDisplayComponent::setMockVitals(int heartRate, int oxygen,
                                          float temperatureC) {
  heartRate_ = heartRate;
  oxygen_ = oxygen;
  temperatureC_ = temperatureC;
}

void SetupDisplayComponent::loop() {
  if (!initialized_) {
    return;
  }

  switch (uiState_) {
    case UiState::kWelcome:
      drawWelcomeFrame();
      if (millis() - stateStartedAtMs_ >= kWelcomeDurationMs) {
        enterState(UiState::kWaitForBluetooth);
        drawBluetoothWaitScreen();
      }
      break;

    case UiState::kWaitForBluetooth:
      drawBluetoothWaitScreen();
      if (bluetooth_ != nullptr && bluetooth_->isConnected()) {
        enterState(UiState::kPromptStand);
        drawPromptScreen("Setup", "Stand up straight", "Press button");
      }
      break;

    case UiState::kPromptStand:
      if (wasButtonPressed()) {
        enterState(UiState::kCountdownToSeat);
        resetCalibrationAccumulator();
        drawCountdownScreen(kSecondPromptDelayMs);
      }
      break;

    case UiState::kCountdownToSeat: {
      accumulateCalibrationSample();
      const uint32_t elapsedMs = millis() - stateStartedAtMs_;
      if (elapsedMs >= kSecondPromptDelayMs) {
        if (calibrationSampleCount_ > 0) {
          standingMeanAngleDeg_ =
              calibrationAngleSumDeg_ / calibrationSampleCount_;
        }
        Serial.printf("Standing mean posture angle: %.2f deg\n",
                      standingMeanAngleDeg_);
        enterState(UiState::kPromptSeat);
        drawCalibrationDoneScreen("Standing saved", standingMeanAngleDeg_);
        delay(1000);
        drawPromptScreen("Setup", "Stand on a seat", "Press button");
      } else {
        drawCountdownScreen(kSecondPromptDelayMs - elapsedMs);
      }
      break;
    }

    case UiState::kPromptSeat:
      if (wasButtonPressed()) {
        enterState(UiState::kCountdownToWalk);
        resetCalibrationAccumulator();
        drawCountdownScreen(kSecondPromptDelayMs);
      }
      break;

    case UiState::kCountdownToWalk: {
      accumulateCalibrationSample();
      const uint32_t elapsedMs = millis() - stateStartedAtMs_;
      if (elapsedMs >= kSecondPromptDelayMs) {
        if (calibrationSampleCount_ > 0) {
          seatedMeanAngleDeg_ = calibrationAngleSumDeg_ / calibrationSampleCount_;
        }
        Serial.printf("Seated mean posture angle: %.2f deg\n",
                      seatedMeanAngleDeg_);
        enterState(UiState::kPromptWalk);
        drawCalibrationDoneScreen("Seated saved", seatedMeanAngleDeg_);
        delay(1000);
        drawPromptScreen("Setup", "Walk slowly", "Press button");
      } else {
        drawCountdownScreen(kSecondPromptDelayMs - elapsedMs);
      }
      break;
    }

    case UiState::kPromptWalk:
      if (wasButtonPressed()) {
        enterState(UiState::kCountdownToReady);
        resetCalibrationAccumulator();
        drawCountdownScreen(kSecondPromptDelayMs);
      }
      break;

    case UiState::kCountdownToReady: {
      accumulateCalibrationSample();
      const uint32_t elapsedMs = millis() - stateStartedAtMs_;
      if (elapsedMs >= kSecondPromptDelayMs) {
        if (calibrationSampleCount_ > 0) {
          walkingMeanAngleDeg_ = calibrationAngleSumDeg_ / calibrationSampleCount_;
        }
        overallMeanAngleDeg_ =
            (standingMeanAngleDeg_ + seatedMeanAngleDeg_ + walkingMeanAngleDeg_) /
            3.0f;
        Serial.printf("Walking mean posture angle: %.2f deg\n",
                      walkingMeanAngleDeg_);
        Serial.printf("Overall mean posture angle: %.2f deg\n",
                      overallMeanAngleDeg_);
        if (gyro_ != nullptr) {
          gyro_->setCalibratedPostureMean(overallMeanAngleDeg_);
        }
        calibrationLoaded_ = saveCalibration();
        enterState(UiState::kDone);
        watchStartedAtMs_ = millis();
        doneScreen_ = DoneScreen::kWatch;
        drawWatchScreen();
      } else {
        drawCountdownScreen(kSecondPromptDelayMs - elapsedMs);
      }
      break;
    }

    case UiState::kDone:
      if (wasButtonPressed()) {
        doneScreen_ = doneScreen_ == DoneScreen::kWatch ? DoneScreen::kVitals
                                                        : DoneScreen::kWatch;
      }
      if (doneScreen_ == DoneScreen::kWatch) {
        drawWatchScreen();
      } else {
        drawVitalsScreen();
      }
      break;
  }
}

void SetupDisplayComponent::drawCenteredText(const String& text, int16_t y,
                                             uint8_t size) {
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  display_.setTextSize(size);
  display_.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  const int16_t x = (kScreenWidth - static_cast<int16_t>(w)) / 2;
  display_.setCursor(x < 0 ? 0 : x, y);
  display_.print(text);
}

void SetupDisplayComponent::drawWelcomeFrame() {
  const uint32_t elapsedMs = millis() - stateStartedAtMs_;

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText("ElderCare", 6, 2);
  drawCenteredText("Booting...", 28, 1);
  display_.drawLine(16, 58, 112, 58, SSD1306_WHITE);
  display_.fillRect(
      16, 58, (elapsedMs % kWelcomeDurationMs) * 96 / kWelcomeDurationMs, 4,
      SSD1306_WHITE);
  display_.display();
}

void SetupDisplayComponent::drawBluetoothWaitScreen() {
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText("Bluetooth", 4, 2);

  String passkeyText = "PIN: ------";
  if (bluetooth_ != nullptr) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "PIN: %06lu",
             static_cast<unsigned long>(bluetooth_->passkey()));
    passkeyText = String(buffer);
  }

  drawCenteredText(passkeyText, 28, 1);
  drawCenteredText("Connect phone", 42, 1);
  drawCenteredText("to continue", 52, 1);
  display_.display();
}

void SetupDisplayComponent::drawPromptScreen(const String& title,
                                             const String& line1,
                                             const String& line2) {
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText(title, 4, 2);
  drawCenteredText(line1, 28, 1);
  drawCenteredText(line2, 42, 1);
  display_.display();
}

void SetupDisplayComponent::drawCountdownScreen(uint32_t remainingMs) {
  const uint32_t secondsLeft = (remainingMs + 999) / 1000;
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText("Hold still", 4, 2);
  drawCenteredText("Collecting posture", 28, 1);
  drawCenteredText(String(secondsLeft) + " sec", 44, 2);
  display_.drawRect(16, 58, 96, 4, SSD1306_WHITE);
  display_.fillRect(16, 58, static_cast<int16_t>(96 * calibrationProgress()), 4,
                    SSD1306_WHITE);
  display_.display();
}

void SetupDisplayComponent::drawCalibrationDoneScreen(const String& label,
                                                      float meanAngleDeg) {
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText(label, 8, 1);
  drawCenteredText(String(meanAngleDeg, 1) + " deg", 28, 2);
  drawCenteredText("Saved", 50, 1);
  display_.display();
}

void SetupDisplayComponent::drawWatchScreen() {
  char timeBuffer[8];
  if (bluetooth_ != nullptr && bluetooth_->hasTimeSync()) {
    uint8_t hours = 0;
    uint8_t minutes = 0;
    bluetooth_->getDisplayTime(hours, minutes);
    snprintf(timeBuffer, sizeof(timeBuffer), "%02u:%02u", hours, minutes);
  } else {
    snprintf(timeBuffer, sizeof(timeBuffer), "--:--");
  }

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  drawCenteredText(String(timeBuffer), 20, 3);
  display_.display();
}

void SetupDisplayComponent::drawVitalsScreen() {
  char tempBuffer[8];
  snprintf(tempBuffer, sizeof(tempBuffer), "%.1f", temperatureC_);

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);

  drawHeartIcon(10, 8);
  display_.setTextSize(2);
  display_.setCursor(28, 6);
  display_.print(heartRate_);

  drawDropIcon(74, 8);
  display_.setCursor(92, 6);
  display_.print(oxygen_);
  display_.print("%");

  drawThermometerIcon(27, 40);
  display_.setTextSize(2);
  display_.setCursor(45, 40);
  display_.print(tempBuffer);
  display_.print("C");

  display_.display();
}

void SetupDisplayComponent::drawHeartIcon(int16_t x, int16_t y) {
  display_.fillCircle(x + 4, y + 4, 3, SSD1306_WHITE);
  display_.fillCircle(x + 10, y + 4, 3, SSD1306_WHITE);
  display_.fillTriangle(x, y + 6, x + 14, y + 6, x + 7, y + 15,
                        SSD1306_WHITE);
}

void SetupDisplayComponent::drawDropIcon(int16_t x, int16_t y) {
  display_.fillTriangle(x + 6, y, x, y + 9, x + 12, y + 9, SSD1306_WHITE);
  display_.fillCircle(x + 6, y + 11, 5, SSD1306_WHITE);
}

void SetupDisplayComponent::drawThermometerIcon(int16_t x, int16_t y) {
  display_.drawRoundRect(x + 4, y, 6, 13, 3, SSD1306_WHITE);
  display_.fillRect(x + 6, y + 3, 2, 7, SSD1306_WHITE);
  display_.fillCircle(x + 7, y + 15, 4, SSD1306_WHITE);
}

void SetupDisplayComponent::enterState(UiState nextState) {
  uiState_ = nextState;
  stateStartedAtMs_ = millis();
}

bool SetupDisplayComponent::wasButtonPressed() {
  const bool reading = digitalRead(kReadyButtonPin);

  if (reading != lastButtonReading_) {
    lastDebounceMs_ = millis();
    lastButtonReading_ = reading;
  }

  if (millis() - lastDebounceMs_ > 30 && reading != stableButtonState_) {
    stableButtonState_ = reading;
    if (stableButtonState_ == LOW) {
      return true;
    }
  }

  return false;
}

const char* SetupDisplayComponent::stateName() const {
  switch (uiState_) {
    case UiState::kWelcome:
      return "welcome";
    case UiState::kWaitForBluetooth:
      return "wait_for_bluetooth";
    case UiState::kPromptStand:
      return "prompt_stand";
    case UiState::kCountdownToSeat:
      return "countdown_to_seat";
    case UiState::kPromptSeat:
      return "prompt_seat";
    case UiState::kCountdownToWalk:
      return "countdown_to_walk";
    case UiState::kPromptWalk:
      return "prompt_walk";
    case UiState::kCountdownToReady:
      return "countdown_to_ready";
    case UiState::kDone:
      return "done";
  }

  return "unknown";
}

bool SetupDisplayComponent::loadCalibration() {
  preferences_.begin(kPreferencesNamespace, true);
  const bool done = preferences_.getBool(kPrefDoneKey, false);
  if (done) {
    standingMeanAngleDeg_ = preferences_.getFloat(kPrefStandKey, NAN);
    seatedMeanAngleDeg_ = preferences_.getFloat(kPrefSeatKey, NAN);
    walkingMeanAngleDeg_ = preferences_.getFloat(kPrefWalkKey, NAN);
    overallMeanAngleDeg_ = preferences_.getFloat(kPrefMeanKey, NAN);
  }
  preferences_.end();
  return done && !isnan(standingMeanAngleDeg_) && !isnan(seatedMeanAngleDeg_) &&
         !isnan(walkingMeanAngleDeg_) && !isnan(overallMeanAngleDeg_);
}

bool SetupDisplayComponent::saveCalibration() {
  if (isnan(standingMeanAngleDeg_) || isnan(seatedMeanAngleDeg_) ||
      isnan(walkingMeanAngleDeg_) || isnan(overallMeanAngleDeg_)) {
    return false;
  }

  preferences_.begin(kPreferencesNamespace, false);
  preferences_.putFloat(kPrefStandKey, standingMeanAngleDeg_);
  preferences_.putFloat(kPrefSeatKey, seatedMeanAngleDeg_);
  preferences_.putFloat(kPrefWalkKey, walkingMeanAngleDeg_);
  preferences_.putFloat(kPrefMeanKey, overallMeanAngleDeg_);
  preferences_.putBool(kPrefDoneKey, true);
  preferences_.end();
  Serial.printf(
      "Calibration saved: standing=%.2f, seated=%.2f, walking=%.2f, mean=%.2f\n",
      standingMeanAngleDeg_, seatedMeanAngleDeg_, walkingMeanAngleDeg_,
      overallMeanAngleDeg_);
  return true;
}

void SetupDisplayComponent::resetCalibrationAccumulator() {
  calibrationAngleSumDeg_ = 0.0f;
  calibrationSampleCount_ = 0;
}

void SetupDisplayComponent::accumulateCalibrationSample() {
  if (gyro_ == nullptr) {
    return;
  }

  calibrationAngleSumDeg_ += gyro_->postureAngleDeg();
  ++calibrationSampleCount_;
}

float SetupDisplayComponent::calibrationProgress() const {
  const float elapsed =
      static_cast<float>(millis() - stateStartedAtMs_) / kSecondPromptDelayMs;
  if (elapsed < 0.0f) {
    return 0.0f;
  }
  if (elapsed > 1.0f) {
    return 1.0f;
  }
  return elapsed;
}
