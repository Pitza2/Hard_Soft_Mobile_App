#include "components/GyroComponent.h"

#include <math.h>
#include <Wire.h>

#include "config/AppConfig.h"

namespace {
constexpr float kGravityMs2 = 9.80665f;
constexpr float kRadToDeg = 57.29578f;
constexpr uint8_t kMpu6050Address = 0x68;
constexpr uint8_t kMpu6050IntPinConfigRegister = 0x37;
constexpr uint8_t kMpu6050IntEnableRegister = 0x38;
constexpr float kFallAccelRangeThresholdMs2 = 5.5f;
constexpr float kFallGyroRangeThresholdDps = 12.0f;
constexpr uint32_t kFallLatchMs = 3000;
constexpr float kMinSampleIntervalSeconds = 0.001f;
constexpr float kClampMin = -1.0f;
constexpr float kClampMax = 1.0f;
}

volatile bool GyroComponent::interruptFired_ = false;

const char* GyroComponent::name() const { return "gyro"; }

bool GyroComponent::isFallDetected() const {
  return fallState_ == FallState::kFallDetected;
}

float GyroComponent::postureAngleDeg() const { return postureAngleDeg_; }

void GyroComponent::setInterruptPin(int interruptPin) {
  interruptPin_ = interruptPin;
}

bool GyroComponent::usingInterrupts() const { return interruptPin_ >= 0; }

void GyroComponent::setCalibratedPostureMean(float meanAngleDeg) {
  fallPostureAngleThresholdDeg_ = meanAngleDeg + 30.0f;
  fallPostureAngleThresholdDegInv_ = meanAngleDeg - 30.0f;
  Serial.printf("Posture thresholds set: upper=%.2f lower=%.2f\n",
                fallPostureAngleThresholdDeg_,
                fallPostureAngleThresholdDegInv_);
}

bool GyroComponent::begin() {
  if (!mpu_.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    return false;
  }

  mpu_.setSampleRateDivisor(19);
  mpu_.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);
  mpu_.setInterruptPinPolarity(false);
  mpu_.setInterruptPinLatch(false);

  if (usingInterrupts()) {
    if (!configureDataReadyInterrupt()) {
      Serial.println("MPU6050 interrupt config failed");
      return false;
    }

    pinMode(interruptPin_, INPUT);
    attachInterrupt(digitalPinToInterrupt(interruptPin_), handleInterrupt,
                    RISING);
    interruptFired_ = true;
  }

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

bool GyroComponent::sampleIfNeeded() {
  if (!usingInterrupts()) {
    return update();
  }

  if (!interruptFired_) {
    return false;
  }

  noInterrupts();
  interruptFired_ = false;
  interrupts();
  return update();
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

  updateFallWindow(nowMs);
  pruneFallWindow(nowMs);
  computeWindowRanges();

  prevAccelXG_ = accelXG;
  prevAccelYG_ = accelYG;
  prevAccelZG_ = accelZG;
  lastSampleAtMs_ = nowMs;

  switch (fallState_) {
    case FallState::kMonitoring:
      if (windowCount_ > 0 &&
          accelRangeMs2_ >= kFallAccelRangeThresholdMs2 &&
          gyroRangeDps_ >= kFallGyroRangeThresholdDps &&
          (postureAngleDeg_ >= fallPostureAngleThresholdDeg_ ||
           postureAngleDeg_ <= fallPostureAngleThresholdDegInv_)) {
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

void GyroComponent::updateFallWindow(uint32_t nowMs) {
  const size_t insertIndex =
      (windowStartIndex_ + windowCount_) % kFallWindowCapacity;
  windowSamples_[insertIndex] = {nowMs, accelMagnitudeG_, gyroMagnitudeDps_};

  if (windowCount_ < kFallWindowCapacity) {
    ++windowCount_;
    return;
  }

  windowStartIndex_ = (windowStartIndex_ + 1) % kFallWindowCapacity;
}

void GyroComponent::pruneFallWindow(uint32_t nowMs) {
  while (windowCount_ > 0) {
    const WindowSample& oldest = windowSamples_[windowStartIndex_];
    if (nowMs - oldest.timestampMs <= kFallWindowMs) {
      break;
    }

    windowStartIndex_ = (windowStartIndex_ + 1) % kFallWindowCapacity;
    --windowCount_;
  }
}

void GyroComponent::computeWindowRanges() {
  if (windowCount_ == 0) {
    accelRangeMs2_ = 0.0f;
    gyroRangeDps_ = 0.0f;
    return;
  }

  const WindowSample& first = windowSamples_[windowStartIndex_];
  float minAccelG = first.accelMagnitudeG;
  float maxAccelG = first.accelMagnitudeG;
  float minGyroDps = first.gyroMagnitudeDps;
  float maxGyroDps = first.gyroMagnitudeDps;

  for (size_t i = 1; i < windowCount_; ++i) {
    const WindowSample& sample =
        windowSamples_[(windowStartIndex_ + i) % kFallWindowCapacity];
    if (sample.accelMagnitudeG < minAccelG) {
      minAccelG = sample.accelMagnitudeG;
    }
    if (sample.accelMagnitudeG > maxAccelG) {
      maxAccelG = sample.accelMagnitudeG;
    }
    if (sample.gyroMagnitudeDps < minGyroDps) {
      minGyroDps = sample.gyroMagnitudeDps;
    }
    if (sample.gyroMagnitudeDps > maxGyroDps) {
      maxGyroDps = sample.gyroMagnitudeDps;
    }
  }

  accelRangeMs2_ = (maxAccelG - minAccelG) * kGravityMs2;
  gyroRangeDps_ = maxGyroDps - minGyroDps;
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

void IRAM_ATTR GyroComponent::handleInterrupt() { interruptFired_ = true; }

bool GyroComponent::configureDataReadyInterrupt() {
  // Use pulsed interrupt behavior so each data-ready event generates a fresh
  // rising edge on the ESP32 interrupt pin.
  return writeRegister(kMpu6050IntPinConfigRegister, 0x00) &&
         writeRegister(kMpu6050IntEnableRegister, 0x01);
}

bool GyroComponent::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kMpu6050Address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
