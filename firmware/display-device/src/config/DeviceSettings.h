#pragma once

#include <Arduino.h>

#include "DeviceConfig.h"

struct DeviceSettings {
  String ssid;
  String password;
  String adminPassword;

  bool isConfigured() const {
    return !ssid.isEmpty() &&
           adminPassword.length() >= DeviceConfig::minAdminPasswordLength;
  }
};
