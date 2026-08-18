#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_OTA

#include "OtaUpdateService.h"

#include <Update.h>

#include "../config/DeviceConfig.h"

esp_err_t OtaUpdateService::handleHttpsUpdate(httpd_req_t* request) {
  char contentType[64] = {};
  if (httpd_req_get_hdr_value_str(request, "Content-Type", contentType,
                                  sizeof(contentType)) != ESP_OK ||
      String(contentType) != "application/octet-stream") {
    httpd_resp_set_status(request, "415 Unsupported Media Type");
    return httpd_resp_send(request, "Expected application/octet-stream",
                           HTTPD_RESP_USE_STRLEN);
  }

  if (request->content_len <= 0 || !Update.begin(request->content_len)) {
    Update.printError(Serial);
    httpd_resp_set_status(request, "400 Bad Request");
    return httpd_resp_send(request, "Firmware image cannot be started",
                           HTTPD_RESP_USE_STRLEN);
  }

  uint8_t buffer[4096];
  size_t remaining = request->content_len;
  while (remaining > 0) {
    const size_t wanted = min(remaining, sizeof(buffer));
    const int received =
        httpd_req_recv(request, reinterpret_cast<char*>(buffer), wanted);
    if (received <= 0 ||
        Update.write(buffer, received) != static_cast<size_t>(received)) {
      Update.printError(Serial);
      Update.abort();
      httpd_resp_set_status(request, "500 Internal Server Error");
      return httpd_resp_send(request, "Firmware upload failed",
                             HTTPD_RESP_USE_STRLEN);
    }
    remaining -= received;
  }

  if (!Update.end(true)) {
    Update.printError(Serial);
    httpd_resp_set_status(request, "500 Internal Server Error");
    return httpd_resp_send(request, "Firmware validation failed",
                           HTTPD_RESP_USE_STRLEN);
  }

  httpd_resp_set_type(request, "text/plain");
  httpd_resp_send(request, "Update installed. Device is rebooting...",
                  HTTPD_RESP_USE_STRLEN);
  delay(DeviceConfig::restartDelayMs);
  ESP.restart();
  return ESP_OK;
}

#endif
