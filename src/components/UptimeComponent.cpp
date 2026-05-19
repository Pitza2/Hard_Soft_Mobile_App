#include "components/UptimeComponent.h"

const char* UptimeComponent::name() const { return "uptime"; }

bool UptimeComponent::begin() { return true; }

bool UptimeComponent::read(String& jsonPayload) {
  jsonPayload = String("{\"millis\":") + String(millis()) + "}";
  return true;
}
