#pragma once

#include <WebServer.h>

#include "../update/OtaUpdateService.h"

class LocalWebServer {
 public:
  void begin(const String& adminPassword);
  void loop();

 private:
  bool authenticate();
  void showHome();
  void showAdmin();

  WebServer server_{80};
  OtaUpdateService otaUpdateService_;
  String adminPassword_;
};
