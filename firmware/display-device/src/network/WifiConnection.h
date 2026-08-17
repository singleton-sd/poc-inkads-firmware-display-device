#pragma once

#include <Arduino.h>

#include "../config/DeviceSettings.h"

class WifiConnection {
 public:
  bool connect(const DeviceSettings& settings) const;
};
