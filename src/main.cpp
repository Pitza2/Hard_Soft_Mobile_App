#include <Arduino.h>

#include "components/BluetoothComponent.h"
#include "components/GyroComponent.h"
#include "components/SetupDisplayComponent.h"

namespace {
BluetoothComponent bluetooth;
GyroComponent gyro;
SetupDisplayComponent setupDisplay;
}

void setup() {
  Serial.begin(115200);
  bluetooth.begin();
  setupDisplay.setBluetoothComponent(&bluetooth);
  setupDisplay.setGyroComponent(&gyro);
  setupDisplay.setMockVitals(72, 98, 36.7f);
  setupDisplay.begin();
  gyro.begin();
}

void loop() {
  bluetooth.loop();
  gyro.update();
  setupDisplay.loop();
  delay(16);
}
