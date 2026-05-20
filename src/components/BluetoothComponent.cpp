#include "components/BluetoothComponent.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEUtils.h>

#include <string>

namespace {
constexpr char kDeviceName[] = "Timisoara 100";
constexpr char kServiceUuid[] = "12345678-1234-1234-1234-123456789abc";
constexpr char kRxCharUuid[] = "abcd1234-5678-90ab-cdef-123456789abc";
constexpr char kTxCharUuid[] = "dcba4321-8765-09ba-fedc-987654321abc";
class ServerCallbacksImpl : public BLEServerCallbacks {
 public:
  explicit ServerCallbacksImpl(BluetoothComponent& owner) : owner_(owner) {}

  void onConnect(BLEServer* server) override {
    (void)server;
    owner_.handleConnected();
  }

  void onDisconnect(BLEServer* server) override {
    owner_.handleDisconnected(server);
  }

 private:
  BluetoothComponent& owner_;
};

class RxCallbacksImpl : public BLECharacteristicCallbacks {
 public:
  explicit RxCallbacksImpl(BluetoothComponent& owner) : owner_(owner) {}

  void onWrite(BLECharacteristic* characteristic) override {
    const std::string rawValue = characteristic->getValue();
    owner_.handleReceived(String(rawValue.c_str()));
  }

 private:
  BluetoothComponent& owner_;
};
}  // namespace

const char* BluetoothComponent::name() const { return "bluetooth"; }

bool BluetoothComponent::begin() {
  BLEDevice::init(kDeviceName);

  server_ = BLEDevice::createServer();
  if (server_ == nullptr) {
    Serial.println("BLE server creation failed");
    return false;
  }
  server_->setCallbacks(new ServerCallbacksImpl(*this));

  BLEService* service = server_->createService(kServiceUuid);
  if (service == nullptr) {
    Serial.println("BLE service creation failed");
    return false;
  }

  txCharacteristic_ = service->createCharacteristic(
      kTxCharUuid, BLECharacteristic::PROPERTY_NOTIFY);
  if (txCharacteristic_ == nullptr) {
    Serial.println("BLE TX characteristic creation failed");
    return false;
  }
  txCharacteristic_->addDescriptor(new BLE2902());

  BLECharacteristic* rxCharacteristic = service->createCharacteristic(
      kRxCharUuid, BLECharacteristic::PROPERTY_WRITE);
  if (rxCharacteristic == nullptr) {
    Serial.println("BLE RX characteristic creation failed");
    return false;
  }
  rxCharacteristic->setCallbacks(new RxCallbacksImpl(*this));

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
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
  lastNotifyMs_ = millis();
  Serial.println("Phone connected");
}

void BluetoothComponent::handleDisconnected(BLEServer* server) {
  deviceConnected_ = false;
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
