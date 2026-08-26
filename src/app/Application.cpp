#include "Application.h"

#include <Arduino.h>

#include "../config/DeviceConfig.h"
#include "../config/InkAdsFeatures.h"
#include "../network/TimeSync.h"
#include "../platform/DeviceIdentity.h"

#if INKADS_FEATURE_EPAPER
#include "../display/DisplayProfile.h"
#include "../display/Framebuffer.h"
#include "../display/FramebufferFixture.h"
#include "../display/Waveshare75BwDisplay.h"
#endif

void Application::begin() {
  Serial.begin(DeviceConfig::serialBaud);
  delay(500);
  Serial.println("Starting InkAds device...");
  Serial.print("Firmware version: ");
  Serial.println(DeviceConfig::firmwareVersion);
  Serial.print("Firmware target: ");
  Serial.println(INKADS_TARGET_ID);
  Serial.print("Device id: ");
  Serial.println(DeviceIdentity::suffix());

#if INKADS_FEATURE_EPAPER
  beginEpaper();
#endif

  const DeviceSettings settings = configStore_.load();
  if (wifiConnection_.connect(settings)) {
    startNormalMode();
  } else {
    startProvisioningMode();
  }
}

void Application::loop() {
  if (deviceMode_ == DeviceMode::Provisioning) {
    provisioningPortal_.loop();
  } else if (deviceMode_ == DeviceMode::Normal) {
    localWebServer_.loop();
  }
  delay(2);
}

void Application::startNormalMode() {
  deviceMode_ = DeviceMode::Normal;
  TimeSync::begin();
  mdnsService_.begin();
  localWebServer_.begin();
  Serial.println("Normal InkAds mode started.");
}

void Application::startProvisioningMode() {
  if (!provisioningPortal_.begin()) {
    Serial.println("Failed to start provisioning access point.");
    delay(DeviceConfig::restartDelayMs);
    ESP.restart();
  }
  deviceMode_ = DeviceMode::Provisioning;
}

#if INKADS_FEATURE_EPAPER
void Application::beginEpaper() {
  if (!epaperDisplay_.begin()) {
    Serial.println("E-paper: failed to initialise panel");
    return;
  }

  static uint8_t fixture[DisplayProfile::kPackedByteLength];
  fillCheckerboardFixture(fixture, sizeof(fixture));
  const FramebufferStatus status =
      epaperDisplay_.displayFramebuffer(fixture, sizeof(fixture));
  if (status != FramebufferStatus::Ok) {
    Serial.print("E-paper: fixture display failed: ");
    Serial.println(framebufferStatusToString(status));
    return;
  }
  Serial.println("E-paper: checkerboard fixture displayed");
}
#endif
