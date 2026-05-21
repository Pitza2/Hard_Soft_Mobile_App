#pragma once

#include <Arduino.h>

#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include "core/Component.h"

class BluetoothComponent;
class GyroComponent;

class SetupDisplayComponent : public Component {
 public:
  SetupDisplayComponent();
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  void loop();
  void setBluetoothComponent(const BluetoothComponent* bluetooth);
  void setGyroComponent(GyroComponent* gyro);
  void setMockVitals(int heartRate, int oxygen, float temperatureC);

 private:
  enum class DoneScreen : uint8_t {
    kWatch,
    kVitals,
  };

  enum class UiState : uint8_t {
    kWelcome,
    kPromptStand,
    kCountdownToSeat,
    kPromptSeat,
    kCountdownToWalk,
    kPromptWalk,
    kCountdownToReady,
    kDone,
  };

  void drawCenteredText(const String& text, int16_t y, uint8_t size);
  void drawWelcomeFrame();
  void drawPromptScreen(const String& title, const String& line1,
                        const String& line2);
  void drawCountdownScreen(uint32_t remainingMs);
  void drawCalibrationDoneScreen(const String& label, float meanAngleDeg);
  void drawWatchScreen();
  void drawVitalsScreen();
  void drawHeartIcon(int16_t x, int16_t y);
  void drawDropIcon(int16_t x, int16_t y);
  void drawThermometerIcon(int16_t x, int16_t y);
  void enterState(UiState nextState);
  bool wasButtonPressed();
  const char* stateName() const;
  bool loadCalibration();
  bool saveCalibration();
  void resetCalibrationAccumulator();
  void accumulateCalibrationSample();
  float calibrationProgress() const;

  Adafruit_SSD1306 display_;
  Preferences preferences_;
  UiState uiState_ = UiState::kWelcome;
  uint32_t stateStartedAtMs_ = 0;
  bool lastButtonReading_ = HIGH;
  bool stableButtonState_ = HIGH;
  uint32_t lastDebounceMs_ = 0;
  bool initialized_ = false;
  uint32_t watchStartedAtMs_ = 0;
  const BluetoothComponent* bluetooth_ = nullptr;
  GyroComponent* gyro_ = nullptr;
  DoneScreen doneScreen_ = DoneScreen::kWatch;
  int heartRate_ = 72;
  int oxygen_ = 98;
  float temperatureC_ = 36.7f;
  bool calibrationLoaded_ = false;
  float standingMeanAngleDeg_ = NAN;
  float seatedMeanAngleDeg_ = NAN;
  float walkingMeanAngleDeg_ = NAN;
  float overallMeanAngleDeg_ = NAN;
  float calibrationAngleSumDeg_ = 0.0f;
  uint32_t calibrationSampleCount_ = 0;
};
