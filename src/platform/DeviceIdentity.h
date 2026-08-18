#pragma once

#include <Arduino.h>

#include "../config/DeviceConfig.h"

namespace DeviceIdentity {
inline String suffix() {
  const uint32_t deviceId = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF);
  char value[7];
  snprintf(value, sizeof(value), "%06lx",
           static_cast<unsigned long>(deviceId));
  return String(value);
}

inline String hostname() { return String("inkads-") + suffix(); }

inline String localUrl() { return String("http://") + hostname() + ".local/"; }

inline String dnsName() {
  return hostname() + "." + String(DeviceConfig::deviceDnsZone);
}

inline String httpsAdminUrl() { return String("https://") + dnsName() + "/admin"; }

inline String setupAccessPointName() {
  String name = DeviceConfig::setupApPrefix;
  name += "-";
  String id = suffix();
  id.toUpperCase();
  name += id;
  return name;
}
}  // namespace DeviceIdentity
