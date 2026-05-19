#pragma once

#include <Arduino.h>

#include "core/Component.h"

class TemperatureComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  float temperatureC() const;

 private:
  float readTemperatureC();
  float lastTemperatureC_ = NAN;
};
