#pragma once

#include <WebServer.h>
#include <esp_http_server.h>

#include "../update/OtaUpdateService.h"

class LocalWebServer {
 public:
  void begin(const String& adminPassword);
  void loop();

 private:
  bool authenticate();
  void showHome();
  void showAdmin();
  void refuseInsecureAdmin();
  bool startHttpsServer();
  static esp_err_t handleHttpsHome(httpd_req_t* request);
  static esp_err_t handleHttpsAdmin(httpd_req_t* request);
  static esp_err_t handleHttpsUpdate(httpd_req_t* request);
  esp_err_t sendHttpsHome(httpd_req_t* request);
  esp_err_t sendHttpsAdmin(httpd_req_t* request);

  WebServer httpServer_{80};
  httpd_handle_t httpsServer_ = nullptr;
  OtaUpdateService otaUpdateService_;
  String adminPassword_;
};
