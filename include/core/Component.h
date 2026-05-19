#pragma once

#include <Arduino.h>

class Component {
 public:
  virtual ~Component() = default;

  virtual const char* name() const = 0;
  virtual bool begin() = 0;
  virtual bool read(String& jsonPayload) = 0;
};
