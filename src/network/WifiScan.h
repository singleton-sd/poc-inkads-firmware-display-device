#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WifiScan {
 public:
  static String toJson();
  static void sendJson(WebServer& server);

 private:
  static String escapeJson(const String& value);
};