#pragma once

#include <Arduino.h>

namespace DeviceConfig {
constexpr uint32_t serialBaud = 115200;
constexpr uint32_t wifiConnectTimeoutMs = 15000;
constexpr uint32_t restartDelayMs = 1500;
constexpr bool debugLogging = true;
constexpr size_t maxSsidLength = 32;
constexpr size_t maxPasswordLength = 63;
constexpr size_t minAdminPasswordLength = 8;
constexpr size_t maxAdminPasswordLength = 63;
constexpr char preferencesNamespace[] = "inkads";
constexpr char ssidKey[] = "wifi_ssid";
constexpr char passwordKey[] = "wifi_pass";
constexpr char adminPasswordKey[] = "admin_pass";
constexpr char setupApPrefix[] = "InkAds-Setup";
constexpr char adminUsername[] = "admin";
constexpr char firmwareVersion[] = "0.2.0";  // x-release-please-version
}  // namespace DeviceConfig
