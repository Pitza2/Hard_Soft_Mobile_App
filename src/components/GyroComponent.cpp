#include "components/GyroComponent.h"

#include <math.h>
#include <Wire.h>

#include "config/AppConfig.h"

namespace {
constexpr float kGravityMs2 = 9.80665f;
constexpr float kRadToDeg = 57.29578f;
constexpr float kDegToRad = 0.01745329f;
constexpr uint8_t kMpu6050Address = 0x68;
constexpr uint8_t kMpu6050IntPinConfigRegister = 0x37;
constexpr uint8_t kMpu6050IntEnableRegister = 0x38;
constexpr float kClampMin = -1.0f;
constexpr float kClampMax = 1.0f;
constexpr float kMinVectorMagnitude = 0.001f;
}

volatile bool GyroComponent::interruptFired_ = false;

const char* GyroComponent::name() const { return "gyro"; }

bool GyroComponent::isFallDetected() const {
  return fallState_ == FallState::kFallDetected;
}

float GyroComponent::postureAngleDeg() const { return postureAngleDeg_; }

uint32_t GyroComponent::stepCount() const { return stepCount_; }

void GyroComponent::setInterruptPin(int interruptPin) {
  interruptPin_ = interruptPin;
}

bool GyroComponent::usingInterrupts() const { return interruptPin_ >= 0; }

void GyroComponent::setCalibratedPostureMean(float meanAngleDeg) {
  fallPostureAngleThresholdDeg_ =
      meanAngleDeg + AppConfig::FALL_POSTURE_THRESHOLD_MARGIN_DEG;
  fallPostureAngleThresholdDegInv_ =
      meanAngleDeg - AppConfig::FALL_POSTURE_THRESHOLD_MARGIN_DEG;
  Serial.printf("Posture thresholds set: upper=%.2f lower=%.2f\n",
                fallPostureAngleThresholdDeg_,
                fallPostureAngleThresholdDegInv_);
}

void GyroComponent::triggerFallDetected() {
  fallState_ = FallState::kFallDetected;
  stateStartedAtMs_ = millis();
  Serial.println("Manual fall trigger activated");
}

void GyroComponent::dismissFallDetected() {
  if (fallState_ != FallState::kFallDetected) {
    return;
  }

  fallState_ = FallState::kMonitoring;
  stateStartedAtMs_ = millis();
  fallCandidateStartedAtMs_ = 0;
  Serial.println("Fall detection dismissed");
}

bool GyroComponent::begin() {
  if (!mpu_.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    return false;
  }

  mpu_.setSampleRateDivisor(AppConfig::MPU6050_SAMPLE_RATE_DIVISOR);
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
                ",\"roll_deg\":" + String(rollDeg_, 1) +
                ",\"pitch_deg\":" + String(pitchDeg_, 1) +
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
  const float accelXG = accelX_ / kGravityMs2;
  const float accelYG = accelY_ / kGravityMs2;
  const float accelZG = accelZ_ / kGravityMs2;
  const float accelMagnitudeMs2 =
      accelMagnitudeG_ > 0.0f ? accelMagnitudeG_ * kGravityMs2 : kGravityMs2;

  jerkMagnitudeGps_ = 0.0f;
  if (hasSample_ && lastSampleAtMs_ != 0) {
    const float sampleIntervalSeconds =
        (nowMs - lastSampleAtMs_) / 1000.0f;
    if (sampleIntervalSeconds >= AppConfig::GYRO_MIN_SAMPLE_INTERVAL_SECONDS) {
      const float jerkX = (accelXG - prevAccelXG_) / sampleIntervalSeconds;
      const float jerkY = (accelYG - prevAccelYG_) / sampleIntervalSeconds;
      const float jerkZ = (accelZG - prevAccelZG_) / sampleIntervalSeconds;
      jerkMagnitudeGps_ = sqrtf(jerkX * jerkX + jerkY * jerkY + jerkZ * jerkZ);
    }
  }

  float postureCos = accelZ_ / accelMagnitudeMs2;
  if (postureCos < kClampMin) {
    postureCos = kClampMin;
  } else if (postureCos > kClampMax) {
    postureCos = kClampMax;
  }
  postureAngleDeg_ = acosf(postureCos) * kRadToDeg;
  if (!postureFilterInitialized_) {
    filteredPostureAngleDeg_ = postureAngleDeg_;
    postureFilterInitialized_ = true;
  } else {
    filteredPostureAngleDeg_ += AppConfig::FALL_POSTURE_FILTER_ALPHA *
                                (postureAngleDeg_ - filteredPostureAngleDeg_);
  }
  rollDeg_ = atan2f(accelY_, accelZ_) * kRadToDeg;
  pitchDeg_ =
      atan2f(-accelX_, sqrtf(accelY_ * accelY_ + accelZ_ * accelZ_)) *
      kRadToDeg;

  if (accelMagnitudeMs2 > kMinVectorMagnitude) {
    const float unitX = accelX_ / accelMagnitudeMs2;
    const float unitY = accelY_ / accelMagnitudeMs2;
    const float unitZ = accelZ_ / accelMagnitudeMs2;

    if (!gravityFilterInitialized_) {
      filteredGravityX_ = unitX;
      filteredGravityY_ = unitY;
      filteredGravityZ_ = unitZ;
      gravityFilterInitialized_ = true;
    } else {
      filteredGravityX_ +=
          AppConfig::FALL_POSTURE_FILTER_ALPHA * (unitX - filteredGravityX_);
      filteredGravityY_ +=
          AppConfig::FALL_POSTURE_FILTER_ALPHA * (unitY - filteredGravityY_);
      filteredGravityZ_ +=
          AppConfig::FALL_POSTURE_FILTER_ALPHA * (unitZ - filteredGravityZ_);
    }

    const float filteredMagnitude =
        sqrtf(filteredGravityX_ * filteredGravityX_ +
              filteredGravityY_ * filteredGravityY_ +
              filteredGravityZ_ * filteredGravityZ_);
    if (filteredMagnitude > kMinVectorMagnitude) {
      filteredGravityX_ /= filteredMagnitude;
      filteredGravityY_ /= filteredMagnitude;
      filteredGravityZ_ /= filteredMagnitude;
    }

    const bool isStableForBaseline =
        gyroMagnitudeDps_ <= AppConfig::POSTURE_BASELINE_STILL_DPS &&
        fabsf(accelMagnitudeG_ - 1.0f) <=
            AppConfig::POSTURE_BASELINE_STILL_ACCEL_DELTA_G;
    if (!baselineGravityInitialized_) {
      baselineGravityX_ = filteredGravityX_;
      baselineGravityY_ = filteredGravityY_;
      baselineGravityZ_ = filteredGravityZ_;
      baselineGravityInitialized_ = true;
    } else if (isStableForBaseline && fallState_ == FallState::kMonitoring) {
      baselineGravityX_ += AppConfig::POSTURE_BASELINE_ALPHA *
                           (filteredGravityX_ - baselineGravityX_);
      baselineGravityY_ += AppConfig::POSTURE_BASELINE_ALPHA *
                           (filteredGravityY_ - baselineGravityY_);
      baselineGravityZ_ += AppConfig::POSTURE_BASELINE_ALPHA *
                           (filteredGravityZ_ - baselineGravityZ_);

      const float baselineMagnitude =
          sqrtf(baselineGravityX_ * baselineGravityX_ +
                baselineGravityY_ * baselineGravityY_ +
                baselineGravityZ_ * baselineGravityZ_);
      if (baselineMagnitude > kMinVectorMagnitude) {
        baselineGravityX_ /= baselineMagnitude;
        baselineGravityY_ /= baselineMagnitude;
        baselineGravityZ_ /= baselineMagnitude;
      }
    }

    float gravityDot = filteredGravityX_ * baselineGravityX_ +
                       filteredGravityY_ * baselineGravityY_ +
                       filteredGravityZ_ * baselineGravityZ_;
    if (gravityDot < kClampMin) {
      gravityDot = kClampMin;
    } else if (gravityDot > kClampMax) {
      gravityDot = kClampMax;
    }
    orientationChangeDeg_ = acosf(gravityDot) * kRadToDeg;
  }

  updateFallWindow();
  computeWindowRanges();

  prevAccelXG_ = accelXG;
  prevAccelYG_ = accelYG;
  prevAccelZG_ = accelZG;
  lastSampleAtMs_ = nowMs;

  switch (fallState_) {
    case FallState::kMonitoring:
      if (accelMagnitudeG_ <= AppConfig::FALL_FREE_FALL_THRESHOLD_G ||
          jerkMagnitudeGps_ >= AppConfig::FALL_IMPACT_JERK_GPS ||
          gyroMagnitudeDps_ >= AppConfig::FALL_ROTATION_GYRO_DPS ||
          (windowCount_ >= kFallWindowCapacity &&
           accelRangeMs2_ >= AppConfig::FALL_ACCEL_RANGE_THRESHOLD_MS2 &&
           gyroRangeDps_ >= AppConfig::FALL_GYRO_RANGE_THRESHOLD_DPS)) {
        fallState_ = FallState::kCandidate;
        stateStartedAtMs_ = nowMs;
        fallCandidateStartedAtMs_ = nowMs;
        impactDetectedAtMs_ = 0;
        stillnessStartedAtMs_ = 0;
        peakOrientationChangeDeg_ = orientationChangeDeg_;
        peakGyroDps_ = gyroMagnitudeDps_;
        peakAccelG_ = accelMagnitudeG_;
      } else {
        fallCandidateStartedAtMs_ = 0;
      }
      break;

    case FallState::kCandidate:
      peakOrientationChangeDeg_ =
          max(peakOrientationChangeDeg_, orientationChangeDeg_);
      peakGyroDps_ = max(peakGyroDps_, gyroMagnitudeDps_);
      peakAccelG_ = max(peakAccelG_, accelMagnitudeG_);

      if (accelMagnitudeG_ >= AppConfig::FALL_IMPACT_ACCEL_G ||
          accelMagnitudeG_ >= AppConfig::FALL_IMPACT_THRESHOLD_G) {
        fallState_ = FallState::kImpactDetected;
        stateStartedAtMs_ = nowMs;
        impactDetectedAtMs_ = nowMs;
        stillnessStartedAtMs_ = 0;
        break;
      }

      if (orientationChangeDeg_ <= AppConfig::FALL_ORIENTATION_RECOVERY_DEG &&
          nowMs - stateStartedAtMs_ >= AppConfig::FALL_ORIENTATION_CHECK_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
        fallCandidateStartedAtMs_ = 0;
      } else if (nowMs - stateStartedAtMs_ > AppConfig::FALL_CONFIRM_WINDOW_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
        fallCandidateStartedAtMs_ = 0;
      }
      break;

    case FallState::kImpactDetected: {
      peakOrientationChangeDeg_ =
          max(peakOrientationChangeDeg_, orientationChangeDeg_);
      peakGyroDps_ = max(peakGyroDps_, gyroMagnitudeDps_);
      peakAccelG_ = max(peakAccelG_, accelMagnitudeG_);

      const bool orientationConfirmed =
          peakOrientationChangeDeg_ >= AppConfig::FALL_ORIENTATION_CHANGE_DEG;
      const bool rotationConfirmed =
          peakGyroDps_ >= AppConfig::FALL_ROTATION_THRESHOLD_DPS;
      const bool impactPersisted =
          nowMs - impactDetectedAtMs_ >= AppConfig::FALL_PERSISTENCE_MS;
      const bool isStillAfterImpact =
          fabsf(accelMagnitudeG_ - 1.0f) <=
              AppConfig::FALL_POST_IMPACT_STILLNESS_G &&
          gyroMagnitudeDps_ <= AppConfig::FALL_POST_IMPACT_STILLNESS_DPS;

      if (orientationConfirmed && rotationConfirmed && impactPersisted &&
          isStillAfterImpact) {
        if (stillnessStartedAtMs_ == 0) {
          stillnessStartedAtMs_ = nowMs;
          break;
        }

        if (nowMs - stillnessStartedAtMs_ >=
            AppConfig::FALL_POST_IMPACT_STILLNESS_MS) {
          Serial.printf(
              "Fall detected: impact=%.2f g peak_gyro=%.1f dps "
              "orientation_change=%.1f deg jerk=%.1f gps\n",
              peakAccelG_, peakGyroDps_, peakOrientationChangeDeg_,
              jerkMagnitudeGps_);
          fallState_ = FallState::kFallDetected;
          stateStartedAtMs_ = nowMs;
          fallCandidateStartedAtMs_ = 0;
          impactDetectedAtMs_ = 0;
          stillnessStartedAtMs_ = 0;
        }
      } else {
        stillnessStartedAtMs_ = 0;
      }

      if (orientationChangeDeg_ <= AppConfig::FALL_ORIENTATION_RECOVERY_DEG &&
          nowMs - impactDetectedAtMs_ >= AppConfig::FALL_ORIENTATION_CHECK_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
        fallCandidateStartedAtMs_ = 0;
        impactDetectedAtMs_ = 0;
        stillnessStartedAtMs_ = 0;
      } else if (nowMs - impactDetectedAtMs_ >
                 AppConfig::FALL_EVALUATION_TIMEOUT_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
        fallCandidateStartedAtMs_ = 0;
        impactDetectedAtMs_ = 0;
        stillnessStartedAtMs_ = 0;
      }
      break;
    }

    case FallState::kFallDetected:
      fallCandidateStartedAtMs_ = 0;
      impactDetectedAtMs_ = 0;
      stillnessStartedAtMs_ = 0;
      if (nowMs - stateStartedAtMs_ > AppConfig::FALL_LATCH_MS) {
        fallState_ = FallState::kMonitoring;
        stateStartedAtMs_ = nowMs;
      }
      break;
  }
}

bool GyroComponent::isPostureInFallZone(float postureAngleDeg) const {
  return postureAngleDeg >= fallPostureAngleThresholdDeg_ ||
         postureAngleDeg <= fallPostureAngleThresholdDegInv_;
}

void GyroComponent::updateFallWindow() {
  const size_t insertIndex =
      (windowStartIndex_ + windowCount_) % kFallWindowCapacity;
  windowSamples_[insertIndex] = {0, accelMagnitudeG_, gyroMagnitudeDps_};

  if (windowCount_ < kFallWindowCapacity) {
    ++windowCount_;
    return;
  }

  windowStartIndex_ = (windowStartIndex_ + 1) % kFallWindowCapacity;
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
    case FallState::kCandidate:
      return "candidate";
    case FallState::kImpactDetected:
      return "impact_detected";
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
