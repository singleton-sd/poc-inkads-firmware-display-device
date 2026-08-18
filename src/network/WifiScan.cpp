#include "WifiScan.h"

#include <WiFi.h>

#include "../config/DeviceConfig.h"

String WifiScan::toJson() {
  const int16_t found = WiFi.scanNetworks(false, false);
  struct Network {
    String ssid;
    int32_t rssi = -127;
    bool secure = true;
  };
  Network networks[DeviceConfig::maxScanResults];
  size_t count = 0;

  for (int16_t index = 0; index < found; ++index) {
    String ssid = WiFi.SSID(index);
    ssid.trim();
    if (ssid.isEmpty() ||
        ssid.startsWith(DeviceConfig::setupApPrefix) ||
        WiFi.channel(index) < 1 || WiFi.channel(index) > 14) {
      continue;
    }

    const int32_t rssi = WiFi.RSSI(index);
    const bool secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    bool merged = false;
    for (size_t existing = 0; existing < count; ++existing) {
      if (networks[existing].ssid == ssid) {
        if (rssi > networks[existing].rssi) {
          networks[existing].rssi = rssi;
          networks[existing].secure = secure;
        }
        merged = true;
        break;
      }
    }
    if (merged) continue;

    if (count < DeviceConfig::maxScanResults) {
      networks[count].ssid = ssid;
      networks[count].rssi = rssi;
      networks[count].secure = secure;
      count += 1;
      continue;
    }

    size_t weakest = 0;
    for (size_t existing = 1; existing < count; ++existing) {
      if (networks[existing].rssi < networks[weakest].rssi) weakest = existing;
    }
    if (rssi > networks[weakest].rssi) {
      networks[weakest].ssid = ssid;
      networks[weakest].rssi = rssi;
      networks[weakest].secure = secure;
    }
  }
  WiFi.scanDelete();

  for (size_t first = 0; first < count; ++first) {
    size_t strongest = first;
    for (size_t next = first + 1; next < count; ++next) {
      if (networks[next].rssi > networks[strongest].rssi) strongest = next;
    }
    if (strongest != first) {
      const Network swapped = networks[first];
      networks[first] = networks[strongest];
      networks[strongest] = swapped;
    }
  }

  String json = "[";
  for (size_t index = 0; index < count; ++index) {
    if (index > 0) json += ",";
    json += "{\"ssid\":\"";
    json += escapeJson(networks[index].ssid);
    json += "\",\"rssi\":";
    json += String(networks[index].rssi);
    json += ",\"secure\":";
    json += networks[index].secure ? "true" : "false";
    json += "}";
  }
  json += "]";
  return json;
}

void WifiScan::sendJson(WebServer& server) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", toJson());
}

String WifiScan::escapeJson(const String& value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (character == '"' || character == '\\') escaped += '\\';
    if (character >= 32) escaped += character;
  }
  return escaped;
}
