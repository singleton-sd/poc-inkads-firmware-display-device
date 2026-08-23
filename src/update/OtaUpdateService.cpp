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
constexpr size_t kDownloadBufferSize = 512;

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

  esp_http_client_config_t config = {};
  config.url = downloadUrl;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = static_cast<int>(DeviceConfig::otaHttpTimeoutMs);
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    error = "HTTP client init failed";
    return false;
  }

  bool ok = false;
  size_t bytesReceived = 0;
  mbedtls_sha256_context shaCtx;
  bool shaStarted = false;

  if (esp_http_client_open(client, 0) != ESP_OK) {
    error = "Download open failed";
    goto cleanup;
  }

  esp_http_client_fetch_headers(client);
  if (esp_http_client_get_status_code(client) != 200) {
    error = "Download failed (status " +
            String(esp_http_client_get_status_code(client)) + ")";
    goto cleanup;
  }

  {
    const int contentLength = esp_http_client_get_content_length(client);
    size_t updateSize = (contentLength > 0) ? static_cast<size_t>(contentLength)
                                            : expectedSize;
    if (updateSize == 0) updateSize = UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize)) {
      Update.printError(Serial);
      error = "Update.begin failed";
      goto cleanup;
    }
  }

  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);
  shaStarted = true;

  {
    char buffer[kDownloadBufferSize];
    while (true) {
      const int readLen = esp_http_client_read(client, buffer, sizeof(buffer));
      if (readLen < 0) {
        error = "Download read failed";
        goto cleanup;
      }
      if (readLen == 0) break;

      if (Update.write(reinterpret_cast<uint8_t*>(buffer), readLen) !=
          static_cast<size_t>(readLen)) {
        Update.printError(Serial);
        error = "Update.write failed";
        goto cleanup;
      }

      mbedtls_sha256_update(&shaCtx,
                            reinterpret_cast<const unsigned char*>(buffer),
                            readLen);
      bytesReceived += static_cast<size_t>(readLen);
    }
  }

  if (expectedSize > 0 && bytesReceived != expectedSize) {
    error = "Size mismatch";
    goto cleanup;
  }

  {
    unsigned char hash[32];
    char calculatedHex[65];
    mbedtls_sha256_finish(&shaCtx, hash);
    CryptoUtil::toHex(hash, 32, calculatedHex, sizeof(calculatedHex));
    if (strcasecmp(calculatedHex, expectedSha256) != 0) {
      error = "SHA256 mismatch";
      goto cleanup;
    }
  }

  if (!Update.end(true)) {
    Update.printError(Serial);
    error = "Firmware validation failed";
    goto cleanup;
  }

  ok = true;

cleanup:
  if (shaStarted) mbedtls_sha256_free(&shaCtx);
  if (!ok && Update.isRunning()) Update.abort();
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return ok;
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
