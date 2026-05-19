#include <Arduino.h>
#include <WiFi.h>

#include "config/AppConfig.h"
#include "core/CloudRunEventClient.h"

namespace {
CloudRunEventClient cloudRunEventClient(
    AppConfig::GOOGLE_SERVICE_ACCOUNT_EMAIL, AppConfig::GOOGLE_PRIVATE_KEY_ID,
    AppConfig::GOOGLE_PRIVATE_KEY_PEM, AppConfig::CLOUD_RUN_URL);

uint32_t lastCloudRunMs = 0;
bool cloudRunAvailable = false;

bool ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.printf("Connecting to WiFi SSID %s\n", AppConfig::WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startMs < AppConfig::WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection timed out");
    return false;
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool shouldSendCloudRunNow() {
  if (!cloudRunAvailable) {
    return false;
  }

  if (millis() - lastCloudRunMs < AppConfig::EVENT_TEST_INTERVAL_MS) {
    return false;
  }

  lastCloudRunMs = millis();
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  cloudRunAvailable = ensureWifiConnected() && cloudRunEventClient.begin();
  if (!cloudRunAvailable) {
    Serial.println("Cloud Run notifier disabled");
  }
}

void loop() {
  if (shouldSendCloudRunNow()) {
    Serial.println("Sending Cloud Run notification");
    if (!ensureWifiConnected() ||
        !cloudRunEventClient.sendNotification("Testing Cloud Run")) {
      Serial.println("Cloud Run notification failed");
    }
  }

  delay(10);
}
