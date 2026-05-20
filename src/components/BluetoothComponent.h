#pragma once

#include <Arduino.h>

#include <BLECharacteristic.h>
#include <BLEServer.h>

#include "core/Component.h"

class BluetoothComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  void loop();

  void handleConnected();
  void handleDisconnected(BLEServer* server);
  void handleReceived(const String& value);

  void sendMessage(const String& message);

 private:

  BLEServer* server_ = nullptr;
  BLECharacteristic* txCharacteristic_ = nullptr;
  bool deviceConnected_ = false;
  uint32_t lastNotifyMs_ = 0;
  uint32_t messageCounter_ = 0;
};
