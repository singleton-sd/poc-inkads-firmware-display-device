#include "OtaUpdateService.h"

#include <Update.h>
#include <mbedtls/base64.h>

#include "../config/DeviceConfig.h"

bool OtaUpdateService::authenticateHttps(httpd_req_t* request,
                                         const String& adminPassword) {
  const size_t headerLength =
      httpd_req_get_hdr_value_len(request, "Authorization");
  if (headerLength > 0 && headerLength < 256) {
    char header[256];
    if (httpd_req_get_hdr_value_str(request, "Authorization", header,
                                    sizeof(header)) == ESP_OK) {
      const String credentials = String(DeviceConfig::adminUsername) + ":" +
                                 adminPassword;
      unsigned char encoded[192];
      size_t encodedLength = 0;
      if (mbedtls_base64_encode(encoded, sizeof(encoded), &encodedLength,
                                reinterpret_cast<const unsigned char*>(
                                    credentials.c_str()),
                                credentials.length()) == 0) {
        const String expected =
            "Basic " + String(reinterpret_cast<char*>(encoded), encodedLength);
        if (constantTimeEquals(String(header), expected)) return true;
      }
    }
  }

  httpd_resp_set_status(request, "401 Unauthorized");
  httpd_resp_set_hdr(request, "WWW-Authenticate",
                     "Basic realm=\"InkAds administration\"");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_send(request, "Authentication required", HTTPD_RESP_USE_STRLEN);
  return false;
}

esp_err_t OtaUpdateService::handleHttpsUpdate(
    httpd_req_t* request, const String& adminPassword) {
  if (!authenticateHttps(request, adminPassword)) return ESP_OK;

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
    const int received = httpd_req_recv(request, reinterpret_cast<char*>(buffer),
                                        wanted);
    if (received <= 0 || Update.write(buffer, received) !=
                             static_cast<size_t>(received)) {
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

bool OtaUpdateService::constantTimeEquals(const String& left,
                                          const String& right) const {
  if (left.length() != right.length()) return false;
  uint8_t difference = 0;
  for (size_t index = 0; index < left.length(); ++index) {
    difference |= static_cast<uint8_t>(left[index] ^ right[index]);
  }
  return difference == 0;
}
