#pragma once

#include "DeviceMode.h"
#include "../config/ConfigStore.h"
#include "../config/InkAdsFeatures.h"
#include "../network/MdnsService.h"
#include "../network/ProvisioningPortal.h"
#include "../network/WifiConnection.h"
#include "../web/LocalWebServer.h"

#if INKADS_FEATURE_EPAPER
#include "../display/Waveshare75BwDisplay.h"
#endif

class Application {
 public:
  void begin();
  void loop();

 private:
  void startNormalMode();
  void startProvisioningMode();
#if INKADS_FEATURE_EPAPER
  void beginEpaper();
#endif

  ConfigStore configStore_;
  WifiConnection wifiConnection_;
  ProvisioningPortal provisioningPortal_{configStore_};
  MdnsService mdnsService_;
  LocalWebServer localWebServer_{configStore_};
  DeviceMode deviceMode_ = DeviceMode::Starting;
#if INKADS_FEATURE_EPAPER
  Waveshare75BwDisplay epaperDisplay_;
#endif
};
