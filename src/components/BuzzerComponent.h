#pragma once

#include <Arduino.h>

#include "core/Component.h"

class BuzzerComponent : public Component {
 public:
  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  void loop();
  void startAlarm();
  void stop();
  bool isActive() const;

 private:
  void playBoth(int frequency);
  void stopBoth();
  bool wasStopButtonPressed();

  bool active_ = false;
  uint32_t lastStepAtMs_ = 0;
  int phase_ = 0;
  int frequencyHz_ = 0;
  int beepCount_ = 0;
  bool stopButtonLastReading_ = HIGH;
  bool stopButtonStableState_ = HIGH;
  uint32_t stopButtonLastDebounceMs_ = 0;
};
