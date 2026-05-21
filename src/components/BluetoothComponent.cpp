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
  BLEDevice::init(kDeviceName);
  BLEDevice::setMTU(247);

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
  sendMessage("{\"action\":\"time\"}");
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
