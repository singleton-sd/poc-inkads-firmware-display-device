#pragma once

#include "DeviceSettings.h"

class ConfigStore {
 public:
  DeviceSettings load() const;
  bool save(const DeviceSettings& settings) const;
  bool clear() const;
};
