#pragma once

#include <Arduino.h>

#include "core/Component.h"

class UptimeComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
};
