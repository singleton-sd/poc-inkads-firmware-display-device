#pragma once

#include <Arduino.h>
#include <time.h>

namespace TimeSync {
inline void begin() {
  configTime(0, 0, "time.windows.com", "pool.ntp.org");
  Serial.println("Synchronizing time for Entra token validation...");
  const uint32_t startedAt = millis();
  while (time(nullptr) < 1700000000 && millis() - startedAt < 10000) {
    delay(250);
  }
  if (time(nullptr) < 1700000000) {
    Serial.println("Time sync not ready; Entra sign-in will wait for NTP.");
  } else {
    Serial.println("Device time synchronized.");
  }
}
}  // namespace TimeSync
