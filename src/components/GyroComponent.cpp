#include "components/GyroComponent.h"

#include <math.h>

#include "config/AppConfig.h"

const char* GyroComponent::name() const { return "gyro"; }

bool GyroComponent::begin() {
  if (!mpu_.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    return false;
  }

  mpu_.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 ready");
  return true;
}

bool GyroComponent::update() {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
  mpu_.getEvent(&accel, &gyro, &temp);

  accelX_ = accel.acceleration.x;
  accelY_ = accel.acceleration.y;
  accelZ_ = accel.acceleration.z;
  gyroX_ = gyro.gyro.x;
  gyroY_ = gyro.gyro.y;
  gyroZ_ = gyro.gyro.z;
  temperatureC_ = temp.temperature;

  const float accelMagnitude =
      sqrtf(accelX_ * accelX_ + accelY_ * accelY_ + accelZ_ * accelZ_);
  accelMagnitudeG_ = accelMagnitude / 9.80665f;

  const float gyroMagnitudeRad =
      sqrtf(gyroX_ * gyroX_ + gyroY_ * gyroY_ + gyroZ_ * gyroZ_);
  gyroMagnitudeDps_ = gyroMagnitudeRad * 57.29578f;

  updateStepCount(accelMagnitudeG_);
  updateFallState(accelMagnitudeG_, gyroMagnitudeDps_);
  hasSample_ = true;
  return true;
}

bool GyroComponent::read(String& jsonPayload) {
  if (!hasSample_ && !update()) {
    return false;
  }

  jsonPayload = String("{\"accel_x\":") + String(accelX_, 3) +
                ",\"accel_y\":" + String(accelY_, 3) +
                ",\"accel_z\":" + String(accelZ_, 3) +
                ",\"gyro_x\":" + String(gyroX_, 3) +
                ",\"gyro_y\":" + String(gyroY_, 3) +
                ",\"gyro_z\":" + String(gyroZ_, 3) +
                ",\"temperature_c\":" + String(temperatureC_, 2) +
                ",\"accel_mag_g\":" + String(accelMagnitudeG_, 3) +
                ",\"gyro_mag_dps\":" + String(gyroMagnitudeDps_, 1) +
                ",\"step_count\":" + String(stepCount_) +
                ",\"fall_detected\":" +
                String(fallState_ == FallState::kFallDetected ? "true"
                                                              : "false") +
                ",\"fall_state\":\"" + String(fallStateName()) + "\"}";
  return true;
}

void GyroComponent::updateStepCount(float accelMagnitudeG) {
  if (!hasSample_) {
    accelBaselineG_ = accelMagnitudeG;
    return;
  }

  accelBaselineG_ =
      accelBaselineG_ +
      AppConfig::STEP_BASELINE_ALPHA * (accelMagnitudeG - accelBaselineG_);

  const float dynamicAccelG = accelMagnitudeG - accelBaselineG_;
  const uint32_t nowMs = millis();

  if (stepPeakArmed_ &&
      dynamicAccelG >= AppConfig::STEP_PEAK_THRESHOLD_G &&
      nowMs - lastStepAtMs_ >= AppConfig::STEP_MIN_INTERVAL_MS) {
    ++stepCount_;
    lastStepAtMs_ = nowMs;
    stepPeakArmed_ = false;
    return;
  }

  if (!stepPeakArmed_ &&
      dynamicAccelG <= AppConfig::STEP_RELEASE_THRESHOLD_G) {
    stepPeakArmed_ = true;
  }
}

void GyroComponent::updateFallState(float accelMagnitudeG,
                                    float gyroMagnitudeDps) {
  const uint32_t nowMs = millis();

  switch (fallState_) {
    case FallState::kMonitoring:
      if (accelMagnitudeG < AppConfig::FALL_FREE_FALL_THRESHOLD_G) {
        fallState_ = FallState::kFreeFallCandidate;
        stateStartedAtMs_ = nowMs;
      }
      break;

    case FallState::kFreeFallCandidate:
      if (nowMs - stateStartedAtMs_ > AppConfig::FALL_CONFIRM_WINDOW_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
        break;
      }

      if (accelMagnitudeG > AppConfig::FALL_IMPACT_THRESHOLD_G &&
          gyroMagnitudeDps > AppConfig::FALL_ROTATION_THRESHOLD_DPS) {
        fallState_ = FallState::kFallDetected;
        stateStartedAtMs_ = nowMs;
      }
      break;

    case FallState::kFallDetected:
      if (nowMs - stateStartedAtMs_ > AppConfig::FALL_LATCH_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
      }
      break;
  }
}

const char* GyroComponent::fallStateName() const {
  switch (fallState_) {
    case FallState::kMonitoring:
      return "monitoring";
    case FallState::kFreeFallCandidate:
      return "free_fall_candidate";
    case FallState::kFallDetected:
      return "fall_detected";
  }

  return "unknown";
}
