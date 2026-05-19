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

 private:
  enum class FallState : uint8_t {
    kMonitoring,
    kFreeFallCandidate,
    kFallDetected,
  };

  void updateStepCount(float accelMagnitudeG);
  void updateFallState(float accelMagnitudeG, float gyroMagnitudeDps);
  const char* fallStateName() const;

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
  float accelBaselineG_ = 1.0f;
  uint32_t stepCount_ = 0;
  uint32_t lastStepAtMs_ = 0;
  bool hasSample_ = false;
  bool stepPeakArmed_ = true;
};
