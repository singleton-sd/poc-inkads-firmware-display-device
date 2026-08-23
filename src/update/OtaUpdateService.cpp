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
#if INKADS_FEATURE_ENTRA
#include "../auth/EntraHttpsClient.h"
#endif
#include "../config/DeviceConfig.h"

namespace {
constexpr size_t kManifestBufferSize = 1024;

struct DownloadContext {
  size_t expectedSize = 0;
  const char* expectedSha256 = nullptr;
  size_t bytesReceived = 0;
  bool updateStarted = false;
  bool failed = false;
  String errorMessage;
  mbedtls_sha256_context shaCtx;
};

esp_err_t otaDownloadEvent(esp_http_client_event_t* evt) {
  auto* ctx = static_cast<DownloadContext*>(evt->user_data);
  if (ctx == nullptr) return ESP_OK;

  if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;

  const int statusCode = esp_http_client_get_status_code(evt->client);
  if (statusCode != 200 || evt->data_len <= 0) return ESP_OK;

  if (!ctx->updateStarted) {
    const int contentLength = esp_http_client_get_content_length(evt->client);
    size_t updateSize = (contentLength > 0)
                            ? static_cast<size_t>(contentLength)
                            : ctx->expectedSize;
    if (updateSize == 0) updateSize = UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize)) {
      ctx->failed = true;
      ctx->errorMessage = "Update.begin failed";
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
    ctx->errorMessage = "Update.write failed";
    Update.printError(Serial);
    Update.abort();
    return ESP_FAIL;
  }

  mbedtls_sha256_update(&ctx->shaCtx,
                        reinterpret_cast<const unsigned char*>(evt->data),
                        evt->data_len);
  ctx->bytesReceived += static_cast<size_t>(evt->data_len);
  return ESP_OK;
}

bool fetchManifestBody(const char* manifestUrl, char* body, size_t bodySize,
                       int* statusCode) {
#if INKADS_FEATURE_ENTRA
  size_t bodyLength = 0;
  EntraHttpsClient https;
  if (!https.get(manifestUrl, body, bodySize, &bodyLength, statusCode)) {
    return false;
  }
  return bodyLength > 0;
#else
  (void)manifestUrl;
  (void)body;
  (void)bodySize;
  if (statusCode != nullptr) *statusCode = 0;
  return false;
#endif
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
      if (*p == '.') {
        ++p;
        break;
      }
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

  char body[kManifestBufferSize];
  int statusCode = 0;
  if (!fetchManifestBody(manifestUrl, body, sizeof(body), &statusCode) ||
      statusCode != 200) {
    result.message =
        "Manifest fetch failed (status " + String(statusCode) + ")";
    return false;
  }

  cJSON* root = cJSON_Parse(body);
  if (root == nullptr) {
    result.message = "Invalid manifest JSON";
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
        "Target " + String(INKADS_TARGET_ID) + " not in manifest";
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
    result.message =
        "Up to date (v" + String(DeviceConfig::firmwareVersion) + ")";
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
  if (expectedSha256 == nullptr || strlen(expectedSha256) == 0) {
    error = "SHA256 checksum is required";
    return false;
  }

  DownloadContext ctx;
  ctx.expectedSize = expectedSize;
  ctx.expectedSha256 = expectedSha256;

  esp_http_client_config_t config = {};
  config.url = downloadUrl;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = static_cast<int>(DeviceConfig::otaHttpTimeoutMs);
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = otaDownloadEvent;
  config.user_data = &ctx;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    error = "HTTP client init failed";
    return false;
  }

  const esp_err_t result = esp_http_client_perform(client);
  const int statusCode = esp_http_client_get_status_code(client);
  const bool complete = esp_http_client_is_complete_data_received(client);
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

  if (!complete) {
    Update.abort();
    mbedtls_sha256_free(&ctx.shaCtx);
    error = "Download truncated";
    return false;
  }

  if (expectedSize > 0 && ctx.bytesReceived != expectedSize) {
    Update.abort();
    mbedtls_sha256_free(&ctx.shaCtx);
    error = "Size mismatch";
    return false;
  }

  unsigned char hash[32];
  char calculatedHex[65];
  mbedtls_sha256_finish(&ctx.shaCtx, hash);
  mbedtls_sha256_free(&ctx.shaCtx);
  CryptoUtil::toHex(hash, 32, calculatedHex, sizeof(calculatedHex));
  if (strcasecmp(calculatedHex, expectedSha256) != 0) {
    Update.abort();
    error = "SHA256 mismatch";
    return false;
  }

  if (!Update.end(true)) {
    Update.printError(Serial);
    error = "Firmware validation failed";
    return false;
  }

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
