#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

class OtaUpdateService {
 public:
  esp_err_t handleHttpsUpdate(httpd_req_t* request);
};