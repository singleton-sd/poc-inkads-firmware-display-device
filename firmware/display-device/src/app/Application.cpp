#include "Application.h"

#include <Arduino.h>

#include "../config/DeviceConfig.h"

void Application::begin() {
  Serial.begin(DeviceConfig::serialBaud);
  delay(500);
  Serial.println("Starting InkAds device...");
  Serial.print("Firmware version: ");
  Serial.println(DeviceConfig::firmwareVersion);

  const DeviceSettings settings = configStore_.load();
  if (wifiConnection_.connect(settings)) {
    startNormalMode(settings);
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

void Application::startNormalMode(const DeviceSettings& settings) {
  deviceMode_ = DeviceMode::Normal;
  mdnsService_.begin();
  localWebServer_.begin(settings.adminPassword);
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
