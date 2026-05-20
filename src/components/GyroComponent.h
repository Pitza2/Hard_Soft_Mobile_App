#pragma once

#include <Arduino.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include "core/Component.h"

class GyroComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  bool update();
  bool isFallDetected() const;

 private:
  enum class FallState : uint8_t {
    kMonitoring,
    kFallDetected,
  };

  void updateStepCount(float accelMagnitudeG);
  void updateFallState();
  const char* fallStateName() const;
  void updateFallWindows();
  float computeWindowRange(const float* values) const;

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
  size_t windowIndex_ = 0;
  size_t windowCount_ = 0;
  bool hasSample_ = false;
  bool stepPeakArmed_ = true;

  static constexpr size_t kFallWindowSize = 50;
  float accelWindow_[kFallWindowSize] = {};
  float gyroWindow_[kFallWindowSize] = {};
};
