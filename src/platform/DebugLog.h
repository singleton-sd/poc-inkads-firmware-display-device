#pragma once

#include <Arduino.h>

#include "../config/DeviceConfig.h"
#include "../config/DeviceSettings.h"

namespace DebugLog {
inline void credentials(const char* stage,
                        const DeviceSettings& credentials) {
  if (!DeviceConfig::debugLogging) return;

  Serial.print("[config] ");
  Serial.println(stage);
  Serial.print("[config] ssid=\"");
  Serial.print(credentials.ssid);
  Serial.println("\"");
  Serial.print("[config] wifi_password_present=");
  Serial.println(credentials.password.isEmpty() ? "false" : "true");
  Serial.print("[config] wifi_password_length=");
  Serial.println(credentials.password.length());
}
}  // namespace DebugLog
