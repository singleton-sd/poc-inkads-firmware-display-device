#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

class OtaUpdateService {
 public:
  bool authenticateHttps(httpd_req_t* request, const String& adminPassword);
  esp_err_t handleHttpsUpdate(httpd_req_t* request,
                              const String& adminPassword);

 private:
  bool constantTimeEquals(const String& left, const String& right) const;
};
