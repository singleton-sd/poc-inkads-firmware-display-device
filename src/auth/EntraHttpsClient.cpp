#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_ENTRA

#include "EntraHttpsClient.h"

#include <cstring>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include "../config/EntraConfig.h"

namespace {
struct BodyCapture {
  char* data = nullptr;
  size_t capacity = 0;
  size_t length = 0;
  bool overflowed = false;
};

esp_err_t captureEvent(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto* capture = static_cast<BodyCapture*>(event->user_data);
  if (capture == nullptr || event->data == nullptr || event->data_len <= 0) {
    return ESP_OK;
  }
  if (capture->length + static_cast<size_t>(event->data_len) >=
      capture->capacity) {
    capture->overflowed = true;
    return ESP_OK;
  }
  memcpy(capture->data + capture->length, event->data, event->data_len);
  capture->length += static_cast<size_t>(event->data_len);
  capture->data[capture->length] = '\0';
  return ESP_OK;
}
}  // namespace

bool EntraHttpsClient::get(const char* url, char* body, size_t bodySize,
                           size_t* bodyLength, int* statusCode) {
  return perform(url, nullptr, body, bodySize, bodyLength, statusCode);
}

bool EntraHttpsClient::postForm(const char* url, const char* form, char* body,
                                size_t bodySize, size_t* bodyLength,
                                int* statusCode) {
  return perform(url, form, body, bodySize, bodyLength, statusCode);
}

bool EntraHttpsClient::perform(const char* url, const char* form, char* body,
                               size_t bodySize, size_t* bodyLength,
                               int* statusCode) {
  if (url == nullptr || body == nullptr || bodySize == 0) return false;
  body[0] = '\0';
  if (bodyLength != nullptr) *bodyLength = 0;
  if (statusCode != nullptr) *statusCode = 0;

  BodyCapture capture;
  capture.data = body;
  capture.capacity = bodySize;

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = form == nullptr ? HTTP_METHOD_GET : HTTP_METHOD_POST;
  config.timeout_ms = static_cast<int>(EntraConfig::httpTimeoutMs);
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = captureEvent;
  config.user_data = &capture;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) return false;

  if (form != nullptr) {
    esp_http_client_set_header(client, "Content-Type",
                               "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, form, strlen(form));
  }

  const esp_err_t result = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (statusCode != nullptr) *statusCode = status;
  if (bodyLength != nullptr) *bodyLength = capture.length;
  return result == ESP_OK && !capture.overflowed && capture.length > 0;
}

#endif
