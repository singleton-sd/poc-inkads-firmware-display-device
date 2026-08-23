#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_OTA

#include "OtaUpdateService.h"

#include <cJSON.h>
#include <cstring>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <mbedtls/sha256.h>
#include <Update.h>

#include "../auth/CryptoUtil.h"
#include "../config/DeviceConfig.h"

namespace {
struct DownloadStreamContext {
  size_t expectedSize = 0;
  String expectedSha256;
  size_t bytesReceived = 0;
  bool updateStarted = false;
  bool failed = false;
  String errorMessage;
  mbedtls_sha256_context shaCtx;
};

esp_err_t otaDownloadEvent(esp_http_client_event_t* evt) {
  auto* ctx = static_cast<DownloadStreamContext*>(evt->user_data);
  if (ctx == nullptr) return ESP_OK;

  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA: {
      const int statusCode = esp_http_client_get_status_code(evt->client);
      if (statusCode != 200) {
        return ESP_OK;
      }
      if (evt->data_len <= 0) return ESP_OK;

      if (!ctx->updateStarted) {
        const int contentLength = esp_http_client_get_content_length(evt->client);
        size_t updateSize = (contentLength > 0)
                                ? static_cast<size_t>(contentLength)
                                : ctx->expectedSize;
        if (updateSize == 0) updateSize = UPDATE_SIZE_UNKNOWN;
        if (!Update.begin(updateSize)) {
          ctx->failed = true;
          ctx->errorMessage = "Update.begin failed (storage partition error)";
          Update.printError(Serial);
          return ESP_FAIL;
        }
        mbedtls_sha256_init(&ctx->shaCtx);
        mbedtls_sha256_starts(&ctx->shaCtx, 0);
        ctx->updateStarted = true;
      }

      if (Update.write(reinterpret_cast<uint8_t*>(evt->data), evt->data_len) !=
          static_cast<size_t>(evt->data_len)) {
        ctx->failed = true;
        ctx->errorMessage = "Update.write failed during stream";
        Update.printError(Serial);
        Update.abort();
        return ESP_FAIL;
      }

      mbedtls_sha256_update(&ctx->shaCtx,
                            reinterpret_cast<const unsigned char*>(evt->data),
                            evt->data_len);
      ctx->bytesReceived += evt->data_len;
      break;
    }
    default:
      break;
  }
  return ESP_OK;
}
}  // namespace

namespace {
struct ManifestCapture {
  char* data = nullptr;
  size_t capacity = 0;
  size_t length = 0;
  bool overflowed = false;
};

esp_err_t manifestCaptureEvent(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto* cap = static_cast<ManifestCapture*>(event->user_data);
  if (cap == nullptr || event->data == nullptr || event->data_len <= 0) {
    return ESP_OK;
  }
  if (cap->length + static_cast<size_t>(event->data_len) >= cap->capacity) {
    cap->overflowed = true;
    return ESP_OK;
  }
  memcpy(cap->data + cap->length, event->data, event->data_len);
  cap->length += static_cast<size_t>(event->data_len);
  cap->data[cap->length] = '\0';
  return ESP_OK;
}
}  // namespace

int OtaUpdateService::compareSemver(const char* a, const char* b) {
  if (a == nullptr && b == nullptr) return 0;
  if (a == nullptr) return -1;
  if (b == nullptr) return 1;

  while (*a == 'v' || *a == 'V') ++a;
  while (*b == 'v' || *b == 'V') ++b;

  auto nextPart = [](const char*& p) -> int {
    while (*p && !isdigit(static_cast<unsigned char>(*p))) {
      if (*p == '.') { ++p; break; }
      ++p;
    }
    int val = 0;
    while (*p && isdigit(static_cast<unsigned char>(*p))) {
      val = val * 10 + (*p - '0');
      ++p;
    }
    if (*p == '.') ++p;
    return val;
  };

  const int aMaj = nextPart(a);
  const int aMin = nextPart(a);
  const int aPat = nextPart(a);

  const int bMaj = nextPart(b);
  const int bMin = nextPart(b);
  const int bPat = nextPart(b);

  if (aMaj != bMaj) return (aMaj > bMaj) ? 1 : -1;
  if (aMin != bMin) return (aMin > bMin) ? 1 : -1;
  if (aPat != bPat) return (aPat > bPat) ? 1 : -1;
  return 0;
}

bool OtaUpdateService::checkReleaseUpdate(const char* manifestUrl,
                                         OtaCheckResult& result) {
  result.currentVersion = DeviceConfig::firmwareVersion;
  result.targetId = INKADS_TARGET_ID;
  result.success = false;
  result.updateAvailable = false;
  result.targetFound = false;
  result.message = "";

  if (manifestUrl == nullptr || strlen(manifestUrl) == 0) {
    result.message = "Manifest URL is empty";
    return false;
  }

  char* body = static_cast<char*>(malloc(4096));
  if (body == nullptr) {
    result.message = "Out of memory allocating manifest buffer";
    return false;
  }
  body[0] = '\0';

  ManifestCapture capture = {body, 4096, 0, false};

  esp_http_client_config_t config = {};
  config.url = manifestUrl;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = static_cast<int>(DeviceConfig::otaHttpTimeoutMs);
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = manifestCaptureEvent;
  config.user_data = &capture;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    free(body);
    result.message = "Could not initialize HTTP client";
    return false;
  }

  const esp_err_t err = esp_http_client_perform(client);
  const int statusCode = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK || statusCode != 200 || capture.overflowed ||
      capture.length == 0) {
    free(body);
    result.message =
        "HTTP manifest request failed (status " + String(statusCode) + ")";
    return false;
  }

  cJSON* root = cJSON_Parse(body);
  free(body);
  if (root == nullptr) {
    result.message = "Failed to parse manifest JSON";
    return false;
  }

  const cJSON* versionItem = cJSON_GetObjectItemCaseSensitive(root, "version");
  const cJSON* channelItem = cJSON_GetObjectItemCaseSensitive(root, "channel");
  const cJSON* targetsItem = cJSON_GetObjectItemCaseSensitive(root, "targets");

  if (!cJSON_IsString(versionItem) || versionItem->valuestring == nullptr ||
      !cJSON_IsObject(targetsItem)) {
    cJSON_Delete(root);
    result.message = "Invalid manifest structure";
    return false;
  }

  result.availableVersion = versionItem->valuestring;
  if (cJSON_IsString(channelItem) && channelItem->valuestring != nullptr) {
    result.channel = channelItem->valuestring;
  }

  const cJSON* targetItem =
      cJSON_GetObjectItemCaseSensitive(targetsItem, INKADS_TARGET_ID);
  if (targetItem != nullptr && cJSON_IsObject(targetItem)) {
    result.targetFound = true;
    const cJSON* urlItem = cJSON_GetObjectItemCaseSensitive(targetItem, "url");
    const cJSON* shaItem = cJSON_GetObjectItemCaseSensitive(targetItem, "sha256");
    const cJSON* sizeItem = cJSON_GetObjectItemCaseSensitive(targetItem, "size");

    if (cJSON_IsString(urlItem) && urlItem->valuestring != nullptr) {
      result.downloadUrl = urlItem->valuestring;
    }
    if (cJSON_IsString(shaItem) && shaItem->valuestring != nullptr) {
      result.sha256 = shaItem->valuestring;
    }
    if (cJSON_IsNumber(sizeItem)) {
      result.size = static_cast<size_t>(sizeItem->valueint);
    }
  }

  cJSON_Delete(root);

  if (!result.targetFound) {
    result.message =
        "Target " + String(INKADS_TARGET_ID) + " not found in release manifest";
    result.success = true;
    result.updateAvailable = false;
    return true;
  }

  const int comp = compareSemver(result.availableVersion.c_str(),
                                 DeviceConfig::firmwareVersion);
  result.updateAvailable = (comp > 0);
  result.success = true;
  if (result.updateAvailable) {
    result.message = "Update available: v" + result.availableVersion;
  } else {
    result.message = "Firmware is up to date (v" +
                     String(DeviceConfig::firmwareVersion) + ")";
  }
  return true;
}

bool OtaUpdateService::installReleaseUpdate(const char* downloadUrl,
                                           const char* expectedSha256,
                                           size_t expectedSize,
                                           String& error) {
  if (downloadUrl == nullptr || strlen(downloadUrl) == 0) {
    error = "Download URL is empty";
    return false;
  }

  DownloadStreamContext ctx;
  ctx.expectedSize = expectedSize;
  if (expectedSha256 != nullptr) {
    ctx.expectedSha256 = expectedSha256;
  }

  esp_http_client_config_t config = {};
  config.url = downloadUrl;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = static_cast<int>(DeviceConfig::otaHttpTimeoutMs);
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = otaDownloadEvent;
  config.user_data = &ctx;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    error = "Could not initialize HTTP client";
    return false;
  }

  Serial.print("Streaming firmware OTA update from: ");
  Serial.println(downloadUrl);

  const esp_err_t result = esp_http_client_perform(client);
  const int statusCode = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (result != ESP_OK || statusCode != 200 || ctx.failed ||
      !ctx.updateStarted) {
    if (ctx.updateStarted) {
      Update.abort();
      mbedtls_sha256_free(&ctx.shaCtx);
    }
    error = ctx.errorMessage.isEmpty()
                ? "Download failed (status " + String(statusCode) + ")"
                : ctx.errorMessage;
    return false;
  }

  if (ctx.expectedSize > 0 && ctx.bytesReceived != ctx.expectedSize) {
    Update.abort();
    mbedtls_sha256_free(&ctx.shaCtx);
    error = "Image size mismatch: received " + String(ctx.bytesReceived) +
            ", expected " + String(ctx.expectedSize);
    return false;
  }

  unsigned char hash[32];
  mbedtls_sha256_finish(&ctx.shaCtx, hash);
  mbedtls_sha256_free(&ctx.shaCtx);

  if (!ctx.expectedSha256.isEmpty()) {
    char calculatedHex[65];
    CryptoUtil::toHex(hash, 32, calculatedHex, sizeof(calculatedHex));
    if (strcasecmp(calculatedHex, ctx.expectedSha256.c_str()) != 0) {
      Update.abort();
      error = "SHA256 mismatch: got " + String(calculatedHex) +
              ", expected " + ctx.expectedSha256;
      return false;
    }
  }

  if (!Update.end(true)) {
    Update.printError(Serial);
    error = "Firmware validation failed on finalize";
    return false;
  }

  Serial.println("OTA firmware update verified and written successfully.");
  return true;
}

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

