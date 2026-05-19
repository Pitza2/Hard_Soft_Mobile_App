#pragma once

#include <Arduino.h>

#include "SparkFun_Bio_Sensor_Hub_Library.h"

#include "core/Component.h"

class PulseOximeterComponent : public Component {
 public:
  PulseOximeterComponent();

  const char* name() const override;
  bool begin() override;
  bool read(String& jsonPayload) override;
  bool sleep();
  bool wake();
  bool hasValidOxygenReading() const;
  int heartRate() const;
  int oxygen() const;
  int status() const;

 private:
  SparkFun_Bio_Sensor_Hub bioHub_;
  bioData lastReading_ {};
  bool hasReading_ = false;
  bool sleeping_ = false;
};
