#include "components/BluetoothComponent.h"

#include <NimBLEDevice.h>

namespace {
constexpr char kDeviceName[] = "Timisoara 100";
constexpr char kServiceUuid[] = "12345678-1234-1234-1234-123456789abc";
constexpr char kRxCharUuid[] = "abcd1234-5678-90ab-cdef-123456789abc";
constexpr char kTxCharUuid[] = "dcba4321-8765-09ba-fedc-987654321abc";
constexpr uint32_t kMinBlePasskey = 100000;
constexpr uint32_t kMaxBlePasskey = 999999;

class ServerCallbacksImpl : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacksImpl(BluetoothComponent& owner) : owner_(owner) {}

  void onConnect(NimBLEServer* server,
                 NimBLEConnInfo& connInfo) override {
    (void)server;
    (void)connInfo;
    owner_.handleConnected();
  }

  void onDisconnect(NimBLEServer* server,
                    NimBLEConnInfo& connInfo,
                    int reason) override {
    (void)connInfo;
    (void)reason;
    owner_.handleDisconnected(server);
  }

  uint32_t onPassKeyDisplay() override { return owner_.passkey(); }

 private:
  BluetoothComponent& owner_;
};

class RxCallbacksImpl : public NimBLECharacteristicCallbacks {
 public:
  explicit RxCallbacksImpl(BluetoothComponent& owner) : owner_(owner) {}

  void onWrite(NimBLECharacteristic* characteristic,
               NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    const std::string rawValue = characteristic->getValue();
    owner_.handleReceived(String(rawValue.c_str()));
  }

 private:
  BluetoothComponent& owner_;
};
}  // namespace

const char* BluetoothComponent::name() const { return "bluetooth"; }

bool BluetoothComponent::isConnected() const { return deviceConnected_; }

bool BluetoothComponent::hasPing() const { return hasPing_; }

uint32_t BluetoothComponent::passkey() const { return passkey_; }

bool BluetoothComponent::hasTimeSync() const { return hasTimeSync_; }

void BluetoothComponent::getDisplayTime(uint8_t& hours, uint8_t& minutes) const {
  if (!hasTimeSync_) {
    hours = 0;
    minutes = 0;
    return;
  }

  const uint32_t elapsedSeconds = (millis() - timeSyncAtMs_) / 1000;
  const uint32_t totalSeconds =
      static_cast<uint32_t>(baseHours_) * 3600 +
      static_cast<uint32_t>(baseMinutes_) * 60 +
      static_cast<uint32_t>(baseSeconds_) + elapsedSeconds;
  hours = (totalSeconds / 3600) % 24;
  minutes = (totalSeconds / 60) % 60;
}

bool BluetoothComponent::begin() {
  passkey_ = kMinBlePasskey + (static_cast<uint32_t>(esp_random()) %
                               (kMaxBlePasskey - kMinBlePasskey + 1));

  Serial.print("BLE passkey: ");
  Serial.println(passkey_);

  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(passkey_);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) {
    Serial.println("BLE server creation failed");
    return false;
  }
  server_->setCallbacks(new ServerCallbacksImpl(*this));

  NimBLEService* service = server_->createService(kServiceUuid);
  if (service == nullptr) {
    Serial.println("BLE service creation failed");
    return false;
  }

  txCharacteristic_ = service->createCharacteristic(
      kTxCharUuid,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY |
          NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::READ_ENC);
  if (txCharacteristic_ == nullptr) {
    Serial.println("BLE TX characteristic creation failed");
    return false;
  }

  NimBLECharacteristic* rxCharacteristic = service->createCharacteristic(
      kRxCharUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_AUTHEN |
                       NIMBLE_PROPERTY::WRITE_ENC);
  if (rxCharacteristic == nullptr) {
    Serial.println("BLE RX characteristic creation failed");
    return false;
  }
  rxCharacteristic->setCallbacks(new RxCallbacksImpl(*this));

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->enableScanResponse(true);
  advertising->start();

  Serial.println("ESP32 BLE ready. Waiting for phone...");
  lastNotifyMs_ = millis();
  messageCounter_ = 0;
  return true;
}

bool BluetoothComponent::read(String& jsonPayload) {
  jsonPayload = String("{\"connected\":") +
                (deviceConnected_ ? "true" : "false") +
                ",\"message_counter\":" + String(messageCounter_) + "}";
  return true;
}

void BluetoothComponent::loop() {
  // Reserved for future non-blocking BLE housekeeping.
}

void BluetoothComponent::handleConnected() {
  deviceConnected_ = true;
  hasPing_ = false;
  lastNotifyMs_ = millis();
  Serial.println("Phone connected");
  sendMessage("{\"action\":\"time\"}");
}

void BluetoothComponent::handleDisconnected(NimBLEServer* server) {
  deviceConnected_ = false;
  hasPing_ = false;
  Serial.println("Phone disconnected");
  delay(500);
  server->startAdvertising();
  Serial.println("Advertising restarted");
}

void BluetoothComponent::handleReceived(const String& value) {
  if (value.isEmpty()) {
    return;
  }

  Serial.print("Received from phone: ");
  Serial.println(value);

  String trimmed = value;
  trimmed.trim();
  if (trimmed.equalsIgnoreCase("ping")) {
    hasPing_ = true;
    Serial.println("Phone handshake received");
    return;
  }

  if (tryHandleTimeSync(value)) {
    return;
  }

  sendMessage(String("ESP received: ") + value);
}

void BluetoothComponent::sendMessage(const String& message) {
  if (!deviceConnected_ || txCharacteristic_ == nullptr) {
    return;
  }

  txCharacteristic_->setValue(message.c_str());
  txCharacteristic_->notify();
  ++messageCounter_;
}

bool BluetoothComponent::tryHandleTimeSync(const String& value) {
  String timeValue = value;
  timeValue.trim();

  if (timeValue.startsWith("TIME:")) {
    timeValue.remove(0, 5);
  } else {
    const int markerIndex = timeValue.indexOf("\"time\":\"");
    if (markerIndex < 0) {
      return false;
    }
    timeValue.remove(0, markerIndex + 8);
    const int closingQuoteIndex = timeValue.indexOf('"');
    if (closingQuoteIndex < 0) {
      return false;
    }
    timeValue = timeValue.substring(0, closingQuoteIndex);
  }

  const int firstColonIndex = timeValue.indexOf(':');
  if (firstColonIndex <= 0) {
    return false;
  }

  const int secondColonIndex = timeValue.indexOf(':', firstColonIndex + 1);
  const uint8_t hours =
      static_cast<uint8_t>(timeValue.substring(0, firstColonIndex).toInt());
  const uint8_t minutes = static_cast<uint8_t>(
      timeValue.substring(firstColonIndex + 1,
                          secondColonIndex >= 0 ? secondColonIndex
                                                : timeValue.length())
          .toInt());
  const uint8_t seconds = static_cast<uint8_t>(
      secondColonIndex >= 0
          ? timeValue.substring(secondColonIndex + 1).toInt()
          : 0);

  if (hours > 23 || minutes > 59 || seconds > 59) {
    return false;
  }

  baseHours_ = hours;
  baseMinutes_ = minutes;
  baseSeconds_ = seconds;
  timeSyncAtMs_ = millis();
  hasTimeSync_ = true;
  Serial.printf("Time synced: %02u:%02u:%02u\n", baseHours_, baseMinutes_,
                baseSeconds_);
  return true;
}
