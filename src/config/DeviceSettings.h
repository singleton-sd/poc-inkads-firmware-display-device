#pragma once

#include <Arduino.h>

struct DeviceSettings {
  String ssid;
  String password;

  bool isConfigured() const { return !ssid.isEmpty(); }
};
