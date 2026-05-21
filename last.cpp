#include <Arduino.h>

#include "components/BluetoothComponent.h"
#include "components/GyroComponent.h"
#include "components/SetupDisplayComponent.h"

namespace {
BluetoothComponent bluetooth;
GyroComponent gyro;
SetupDisplayComponent setupDisplay;

constexpr int kGyroInterruptPin = 19;
constexpr uint32_t kGyroDebugPrintEverySamples = 20;

uint32_t gyroSampleCounter = 0;
}

void setup() {
  Serial.begin(115200);
  bluetooth.begin();
  gyro.setInterruptPin(kGyroInterruptPin);
  setupDisplay.setBluetoothComponent(&bluetooth);
  setupDisplay.setGyroComponent(&gyro);
  setupDisplay.setMockVitals(72, 98, 36.7f);
  setupDisplay.begin();
  gyro.begin();
}

void loop() {
  bluetooth.loop();
  if (gyro.sampleIfNeeded()) {
    ++gyroSampleCounter;
    if (gyroSampleCounter % kGyroDebugPrintEverySamples == 0) {
      String gyroPayload;
      if (gyro.read(gyroPayload)) {
        Serial.print("Gyro sample ");
        Serial.print(gyroSampleCounter);
        Serial.print(": ");
        Serial.println(gyroPayload);
      }
    }
  }
  setupDisplay.loop();
  delay(16);
}
