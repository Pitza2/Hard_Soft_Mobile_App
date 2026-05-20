#include "components/GyroComponent.h"

#include <math.h>

#include "config/AppConfig.h"

namespace {
constexpr float kGravityMs2 = 9.80665f;
constexpr float kRadToDeg = 57.29578f;
constexpr float kFallAccelRangeThresholdMs2 = 5.5f;
constexpr float kFallGyroRangeThresholdDps = 12.0f;
constexpr float kFallPostureAngleThresholdDeg = 120.0f;   // Changed during calibration
constexpr float kFallPostureAngleThresholdDegInv = 45.0f;
constexpr uint32_t kFallLatchMs = 3000;
constexpr float kMinSampleIntervalSeconds = 0.001f;
constexpr float kClampMin = -1.0f;
constexpr float kClampMax = 1.0f;
}

const char* GyroComponent::name() const { return "gyro"; }

bool GyroComponent::isFallDetected() const {
  return fallState_ == FallState::kFallDetected;
}

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
  accelMagnitudeG_ = accelMagnitude / kGravityMs2;

  const float gyroMagnitudeRad =
      sqrtf(gyroX_ * gyroX_ + gyroY_ * gyroY_ + gyroZ_ * gyroZ_);
  gyroMagnitudeDps_ = gyroMagnitudeRad * kRadToDeg;

  updateStepCount(accelMagnitudeG_);
  updateFallState();
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
                ",\"jerk_mag_gps\":" + String(jerkMagnitudeGps_, 3) +
                ",\"accel_range_ms2\":" + String(accelRangeMs2_, 3) +
                ",\"gyro_range_dps\":" + String(gyroRangeDps_, 1) +
                ",\"posture_angle_deg\":" + String(postureAngleDeg_, 1) +
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

void GyroComponent::updateFallState() {
  const uint32_t nowMs = millis();
  const float accelXG = accelX_ / kGravityMs2; // Convert from m/s^2 to g units (values easier to understand)
  const float accelYG = accelY_ / kGravityMs2;
  const float accelZG = accelZ_ / kGravityMs2;
  const float accelMagnitudeMs2 =             // Convert back from g to m/s^2
      accelMagnitudeG_ > 0.0f ? accelMagnitudeG_ * kGravityMs2 : kGravityMs2;

  jerkMagnitudeGps_ = 0.0f;
  if (hasSample_ && lastSampleAtMs_ != 0) {
    const float sampleIntervalSeconds =
        (nowMs - lastSampleAtMs_) / 1000.0f;
    if (sampleIntervalSeconds >= kMinSampleIntervalSeconds) {
      const float jerkX = (accelXG - prevAccelXG_) / sampleIntervalSeconds;
      const float jerkY = (accelYG - prevAccelYG_) / sampleIntervalSeconds;
      const float jerkZ = (accelZG - prevAccelZG_) / sampleIntervalSeconds;
      jerkMagnitudeGps_ = sqrtf(jerkX * jerkX + jerkY * jerkY + jerkZ * jerkZ);
    }
  }

  float postureCos = accelZ_ / accelMagnitudeMs2; // Calculates posture angle of the device, cos = adj / hypotenuse (Cosine of the angle between phone and the total aceleration vector)
  if (postureCos < kClampMin) {
    postureCos = kClampMin;
  } else if (postureCos > kClampMax) {
    postureCos = kClampMax;
  }
  postureAngleDeg_ = acosf(postureCos) * kRadToDeg; // Convert cosine to an angle in radians and then to degrees

  updateFallWindows(); // Save accelerometer and gyroscope magnitude inside a 50 samples window (1 second)
  // How much the acceleration and rotation speed changes in the window
  accelRangeMs2_ = computeWindowRange(accelWindow_) * kGravityMs2; // Calculates MAX-MIN in the recent window (convert from g units to m/s^2)
  gyroRangeDps_ = computeWindowRange(gyroWindow_);

  prevAccelXG_ = accelXG;
  prevAccelYG_ = accelYG;
  prevAccelZG_ = accelZG;
  lastSampleAtMs_ = nowMs;

  switch (fallState_) {
    case FallState::kMonitoring:
      if (windowCount_ == kFallWindowSize &&
          accelRangeMs2_ >= kFallAccelRangeThresholdMs2 &&
          gyroRangeDps_ >= kFallGyroRangeThresholdDps &&
          (postureAngleDeg_ >= kFallPostureAngleThresholdDeg || postureAngleDeg_ <= kFallPostureAngleThresholdDegInv)) {
        Serial.printf(
            "Fall detected: accel_range=%.2f m/s^2 gyro_range=%.2f dps "
            "posture=%.1f deg\n",
            accelRangeMs2_, gyroRangeDps_, postureAngleDeg_);
        fallState_ = FallState::kFallDetected;
        stateStartedAtMs_ = nowMs;
      }
      break;

    case FallState::kFallDetected:
      if (nowMs - stateStartedAtMs_ > kFallLatchMs) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
      }
      break;
  }
}

void GyroComponent::updateFallWindows() {
  accelWindow_[windowIndex_] = accelMagnitudeG_;
  gyroWindow_[windowIndex_] = gyroMagnitudeDps_;
  windowIndex_ = (windowIndex_ + 1) % kFallWindowSize;
  if (windowCount_ < kFallWindowSize) {
    ++windowCount_;
  }
}

float GyroComponent::computeWindowRange(const float* values) const {
  if (windowCount_ == 0) {
    return 0.0f;
  }

  float minValue = values[0];
  float maxValue = values[0];
  for (size_t i = 1; i < windowCount_; ++i) {
    if (values[i] < minValue) {
      minValue = values[i];
    }
    if (values[i] > maxValue) {
      maxValue = values[i];
    }
  }

  return maxValue - minValue;
}

const char* GyroComponent::fallStateName() const {
  switch (fallState_) {
    case FallState::kMonitoring:
      return "monitoring";
    case FallState::kFallDetected:
      return "fall_detected";
  }

  return "unknown";
}
