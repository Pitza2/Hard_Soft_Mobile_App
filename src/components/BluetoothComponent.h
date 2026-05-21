#pragma once

#include <Arduino.h>

#include <NimBLECharacteristic.h>
#include <NimBLEServer.h>

#include "core/Component.h"

class BluetoothComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  void loop();

  void handleConnected();
  void handleDisconnected(NimBLEServer* server);
  void handleReceived(const String& value);

  void sendMessage(const String& message);
  bool isConnected() const;
  bool hasPing() const;
  uint32_t passkey() const;
  bool hasTimeSync() const;
  void getDisplayTime(uint8_t& hours, uint8_t& minutes) const;

 private:
  bool tryHandleTimeSync(const String& value);

  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* txCharacteristic_ = nullptr;
  bool deviceConnected_ = false;
  bool hasPing_ = false;
  uint32_t lastNotifyMs_ = 0;
  uint32_t messageCounter_ = 0;
  uint32_t passkey_ = 0;
  bool hasTimeSync_ = false;
  uint8_t baseHours_ = 0;
  uint8_t baseMinutes_ = 0;
  uint8_t baseSeconds_ = 0;
  uint32_t timeSyncAtMs_ = 0;
};
