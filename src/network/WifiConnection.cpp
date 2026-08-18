#include "WifiConnection.h"

#include <WiFi.h>

#include "../config/DeviceConfig.h"

bool WifiConnection::connect(const DeviceSettings& settings) const {
  if (!settings.isConfigured()) {
    Serial.println("No complete saved configuration was found.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid.c_str(), settings.password.c_str());

  Serial.print("Connecting to saved Wi-Fi");
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < DeviceConfig::wifiConnectTimeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Saved Wi-Fi unavailable; entering provisioning mode.");
    return false;
  }

  Serial.print("Connected. Device IP: ");
  Serial.println(WiFi.localIP());
  return true;
}
