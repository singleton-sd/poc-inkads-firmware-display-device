#include "MdnsService.h"

#include <Arduino.h>
#include <ESPmDNS.h>

#include "../platform/DeviceIdentity.h"

bool MdnsService::begin() const {
  const String hostname = DeviceIdentity::hostname();
  if (!MDNS.begin(hostname.c_str())) {
    Serial.println("mDNS startup failed; use the numeric IP address.");
    return false;
  }

  MDNS.addService("http", "tcp", 80);
  Serial.print("Local hostname: ");
  Serial.println(DeviceIdentity::localUrl());
  return true;
}
