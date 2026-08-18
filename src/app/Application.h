#pragma once

#include "DeviceMode.h"
#include "../config/ConfigStore.h"
#include "../network/MdnsService.h"
#include "../network/ProvisioningPortal.h"
#include "../network/WifiConnection.h"
#include "../web/LocalWebServer.h"

class Application {
 public:
  void begin();
  void loop();

 private:
  void startNormalMode(const DeviceSettings& settings);
  void startProvisioningMode();

  ConfigStore configStore_;
  WifiConnection wifiConnection_;
  ProvisioningPortal provisioningPortal_{configStore_};
  MdnsService mdnsService_;
  LocalWebServer localWebServer_;
  DeviceMode deviceMode_ = DeviceMode::Starting;
};
