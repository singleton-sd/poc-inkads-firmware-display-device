#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

struct OtaCheckResult {
  bool success = false;
  String currentVersion;
  String targetId;
  String availableVersion;
  String channel;
  String downloadUrl;
  String sha256;
  size_t size = 0;
  bool updateAvailable = false;
  bool targetFound = false;
  String message;
};

class OtaUpdateService {
 public:
  static int compareSemver(const char* versionA, const char* versionB);

  bool checkReleaseUpdate(const char* manifestUrl, OtaCheckResult& result);
  bool installReleaseUpdate(const char* downloadUrl, const char* expectedSha256,
                            size_t expectedSize, String& error);

  esp_err_t handleHttpsUpdate(httpd_req_t* request);
};