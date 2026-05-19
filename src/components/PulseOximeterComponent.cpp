#include "components/PulseOximeterComponent.h"

#include "config/AppConfig.h"

PulseOximeterComponent::PulseOximeterComponent()
    : bioHub_(AppConfig::BIO_SENSOR_RESET_PIN, AppConfig::BIO_SENSOR_MFIO_PIN) {
}

const char* PulseOximeterComponent::name() const { return "pulse_oximeter"; }

bool PulseOximeterComponent::begin() {
  const int beginResult = bioHub_.begin();
  if (beginResult != 0) {
    Serial.printf("Pulse oximeter begin failed: %d\n", beginResult);
    return false;
  }

  const int configResult = bioHub_.configBpm(MODE_ONE);
  if (configResult != 0) {
    Serial.printf("Pulse oximeter config failed: %d\n", configResult);
    return false;
  }

  delay(AppConfig::PULSE_OXIMETER_STARTUP_DELAY_MS);
  sleeping_ = false;
  Serial.println("Pulse oximeter ready");
  return true;
}

bool PulseOximeterComponent::read(String& jsonPayload) {
  if (sleeping_) {
    return false;
  }

  lastReading_ = bioHub_.readBpm();
  hasReading_ = true;

  jsonPayload = String("{\"heart_rate\":") + String(lastReading_.heartRate) +
                ",\"confidence\":" + String(lastReading_.confidence) +
                ",\"oxygen\":" + String(lastReading_.oxygen) +
                ",\"status\":" + String(lastReading_.status) +
                ",\"ext_status\":" + String(lastReading_.extStatus) + "}";
  return true;
}

bool PulseOximeterComponent::sleep() {
  if (sleeping_) {
    return true;
  }

  const uint8_t status = bioHub_.max30101Control(0);
  if (status != 0) {
    Serial.printf("Pulse oximeter sleep failed: %u\n", status);
    return false;
  }

  sleeping_ = true;
  return true;
}

bool PulseOximeterComponent::wake() {
  if (!sleeping_) {
    return true;
  }

  const uint8_t status = bioHub_.max30101Control(1);
  if (status != 0) {
    Serial.printf("Pulse oximeter wake failed: %u\n", status);
    return false;
  }

  delay(AppConfig::PULSE_OXIMETER_STARTUP_DELAY_MS);
  sleeping_ = false;
  return true;
}

bool PulseOximeterComponent::hasValidOxygenReading() const {
  return hasReading_ && lastReading_.status == 3 && lastReading_.oxygen > 0;
}

int PulseOximeterComponent::heartRate() const {
  return hasReading_ ? lastReading_.heartRate : 0;
}

int PulseOximeterComponent::oxygen() const {
  return hasReading_ ? lastReading_.oxygen : 0;
}

int PulseOximeterComponent::status() const {
  return hasReading_ ? lastReading_.status : 0;
}
