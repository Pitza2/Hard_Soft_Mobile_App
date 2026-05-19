#pragma once

#include <Arduino.h>

#include "core/Component.h"

class BuzzerComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  void beepFor(uint32_t durationMs);
 void stop();

 private:
  bool buzzerOn_ = false;
  bool toneStarted_ = false;
};
