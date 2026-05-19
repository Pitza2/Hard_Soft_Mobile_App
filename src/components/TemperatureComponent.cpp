#include "components/TemperatureComponent.h"

#include <Wire.h>
#include <math.h>

#include "config/AppConfig.h"

const char* TemperatureComponent::name() const { return "body_temp"; }

bool TemperatureComponent::begin() {
  Wire.beginTransmission(AppConfig::MAX30205_ADDRESS);
  const byte error = Wire.endTransmission();

  if (error != 0) {
    Serial.printf("MAX30205 not available, I2C error: %u\n", error);
    return false;
  }

  Serial.println("MAX30205 ready");
  return true;
}

bool TemperatureComponent::read(String& jsonPayload) {
  const float temperatureC = readTemperatureC();
  if (isnan(temperatureC)) {
    return false;
  }

  lastTemperatureC_ = temperatureC;
  jsonPayload = String("{\"temperature_c\":") + String(temperatureC, 2) + "}";
  return true;
}

float TemperatureComponent::temperatureC() const { return lastTemperatureC_; }

float TemperatureComponent::readTemperatureC() {
  Wire.beginTransmission(AppConfig::MAX30205_ADDRESS);
  Wire.write(AppConfig::MAX30205_TEMP_REGISTER);

  const byte error = Wire.endTransmission(false);
  if (error != 0) {
    Serial.printf("MAX30205 write error: %u\n", error);
    return NAN;
  }

  const int bytesRead =
      Wire.requestFrom(static_cast<uint8_t>(AppConfig::MAX30205_ADDRESS),
                       static_cast<uint8_t>(2));
  if (bytesRead != 2) {
    Serial.printf("MAX30205 read failed, bytes read: %d\n", bytesRead);
    return NAN;
  }

  const uint8_t msb = Wire.read();
  const uint8_t lsb = Wire.read();
  const int16_t raw = (static_cast<int16_t>(msb) << 8) | lsb;
  return raw / 256.0f + 64.0;
}
