#include <Arduino.h>

#include "components/GyroComponent.h"

namespace {
GyroComponent gyro;

constexpr int kGyroInterruptPin = 19;
}

void setup() {
  Serial.begin(115200);
  gyro.setInterruptPin(kGyroInterruptPin);

  if (!gyro.begin()) {
    Serial.println("Gyro init failed");
    return;
  }

  Serial.println("Gyro sampling started");
}

void loop() {
  if (!gyro.sampleIfNeeded()) {
    delay(1);
    return;
  }

  String gyroPayload;
  if (!gyro.read(gyroPayload)) {
    Serial.println("Gyro payload build failed");
    return;
  }

  Serial.println(gyroPayload);
}
