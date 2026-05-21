#pragma once

#include <Arduino.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include "config/AppConfig.h"
#include "core/Component.h"

class GyroComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  bool update();
  bool sampleIfNeeded();
  bool isFallDetected() const;
  float postureAngleDeg() const;
  uint32_t stepCount() const;
  void setInterruptPin(int interruptPin);
  bool usingInterrupts() const;
  void setCalibratedPostureMean(float meanAngleDeg);
  void triggerFallDetected();

 private:
  struct WindowSample {
    uint32_t timestampMs;
    float accelMagnitudeG;
    float gyroMagnitudeDps;
  };

  static void IRAM_ATTR handleInterrupt();
  enum class FallState : uint8_t {
    kMonitoring,
    kFallDetected,
  };

  bool configureDataReadyInterrupt();
  bool writeRegister(uint8_t reg, uint8_t value);
  void updateStepCount(float accelMagnitudeG);
  void updateFallState();
  const char* fallStateName() const;
  void updateFallWindow(uint32_t nowMs);
  void pruneFallWindow(uint32_t nowMs);
  void computeWindowRanges();

  Adafruit_MPU6050 mpu_;
  FallState fallState_ = FallState::kMonitoring;
  uint32_t stateStartedAtMs_ = 0;
  float accelX_ = 0.0f;
  float accelY_ = 0.0f;
  float accelZ_ = 0.0f;
  float gyroX_ = 0.0f;
  float gyroY_ = 0.0f;
  float gyroZ_ = 0.0f;
  float temperatureC_ = 0.0f;
  float accelMagnitudeG_ = 0.0f;
  float gyroMagnitudeDps_ = 0.0f;
  float postureAngleDeg_ = 0.0f;
  float rollDeg_ = 0.0f;
  float pitchDeg_ = 0.0f;
  float fallPostureAngleThresholdDeg_ =
      90.0f + AppConfig::FALL_POSTURE_THRESHOLD_MARGIN_DEG;
  float fallPostureAngleThresholdDegInv_ =
      90.0f - AppConfig::FALL_POSTURE_THRESHOLD_MARGIN_DEG;
  float accelRangeMs2_ = 0.0f;
  float gyroRangeDps_ = 0.0f;
  float prevAccelXG_ = 0.0f;
  float prevAccelYG_ = 0.0f;
  float prevAccelZG_ = 0.0f;
  float jerkMagnitudeGps_ = 0.0f;
  float accelBaselineG_ = 1.0f;
  uint32_t stepCount_ = 0;
  uint32_t lastStepAtMs_ = 0;
  uint32_t lastSampleAtMs_ = 0;
  size_t windowStartIndex_ = 0;
  size_t windowCount_ = 0;
  int interruptPin_ = -1;
  bool hasSample_ = false;
  bool stepPeakArmed_ = true;

  static volatile bool interruptFired_;
  static constexpr uint32_t kFallWindowMs = AppConfig::FALL_WINDOW_MS;
  static constexpr size_t kFallWindowCapacity =
      AppConfig::FALL_WINDOW_CAPACITY;
  WindowSample windowSamples_[kFallWindowCapacity] = {};
};
