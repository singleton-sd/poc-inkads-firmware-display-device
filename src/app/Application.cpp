#include "Application.h"

#include <Arduino.h>

#include "../config/DeviceConfig.h"
#include "../network/TimeSync.h"

void Application::begin() {
  Serial.begin(DeviceConfig::serialBaud);
  delay(500);
  Serial.println("Starting InkAds device...");
  Serial.print("Firmware version: ");
  Serial.println(DeviceConfig::firmwareVersion);

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
