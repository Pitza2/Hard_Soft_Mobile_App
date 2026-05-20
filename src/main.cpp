#include <Arduino.h>

#include "components/BluetoothComponent.h"

namespace {
BluetoothComponent bluetooth;
}

void setup() {
  Serial.begin(115200);
  bluetooth.begin();
}

void loop() {
  bluetooth.sendMessage(String("Tetstare!"));

  delay(2000);
}
