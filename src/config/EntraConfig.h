#pragma once

#include <Arduino.h>
#include <cstdio>

// These identifiers are public OAuth client configuration, not secrets.
namespace EntraConfig {
inline constexpr char tenantId[] = "9a0e57d7-e58e-4e8b-814d-037cd7d9015c";
inline constexpr char clientId[] = "16082da8-3f1f-4be7-a172-421b8cb21f25";
inline constexpr char administratorRole[] = "InkAds.Admin";
inline constexpr char host[] = "login.microsoftonline.com";
inline constexpr uint32_t sessionInactivityTimeoutMs = 10 * 60 * 1000;
inline constexpr uint32_t sessionAbsoluteTimeoutMs = 30 * 60 * 1000;
inline constexpr int clockSkewSeconds = 60;
inline constexpr uint32_t httpTimeoutMs = 15000;
inline constexpr size_t maxHttpBodyBytes = 12288;

inline bool configured() {
  return tenantId[0] != '\0' && clientId[0] != '\0';
}

inline void authorityUrl(char* out, size_t outSize, const char* path) {
  snprintf(out, outSize, "https://%s/%s%s", host, tenantId, path);
}

inline void expectedIssuer(char* out, size_t outSize) {
  snprintf(out, outSize, "https://%s/%s/v2.0", host, tenantId);
}

inline void scope(char* out, size_t outSize) {
  snprintf(out, outSize, "openid%%20%s/.default", clientId);
}
}  // namespace EntraConfig
