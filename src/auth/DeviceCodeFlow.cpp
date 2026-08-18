#include "DeviceCodeFlow.h"

#include <cstring>
#include <WiFi.h>
#include <cJSON.h>

#include "../config/DeviceConfig.h"
#include "../config/EntraConfig.h"
#include "CryptoUtil.h"

namespace {
void copyJsonString(cJSON* object, const char* name, char* out, size_t outSize) {
  if (outSize == 0) return;
  out[0] = '\0';
  cJSON* field = cJSON_GetObjectItemCaseSensitive(object, name);
  if (!cJSON_IsString(field) || field->valuestring == nullptr) return;
  strncpy(out, field->valuestring, outSize - 1);
  out[outSize - 1] = '\0';
}

char* duplicateJsonString(cJSON* object, const char* name) {
  cJSON* field = cJSON_GetObjectItemCaseSensitive(object, name);
  if (!cJSON_IsString(field) || field->valuestring == nullptr) return nullptr;
  const size_t length = strlen(field->valuestring);
  char* copy = static_cast<char*>(malloc(length + 1));
  if (copy == nullptr) return nullptr;
  memcpy(copy, field->valuestring, length + 1);
  return copy;
}

void logState(const char* text) {
  if (!DeviceConfig::debugLogging) return;
  Serial.print("[entra] ");
  Serial.println(text);
}
}  // namespace

void DeviceCodeFlow::requestStart() {
  if (state_ == DeviceCodeState::Pending) return;
  wipeTokens();
  clearSensitive();
  state_ = DeviceCodeState::StartRequested;
}

void DeviceCodeFlow::cancel() {
  wipeTokens();
  clearSensitive();
  state_ = DeviceCodeState::Cancelled;
  logState("cancelled");
}

void DeviceCodeFlow::reject() {
  wipeTokens();
  clearSensitive();
  state_ = DeviceCodeState::Denied;
  logState("denied");
}

void DeviceCodeFlow::resetToIdle() {
  wipeTokens();
  clearSensitive();
  state_ = DeviceCodeState::Idle;
}

void DeviceCodeFlow::loop() {
  if (state_ == DeviceCodeState::StartRequested) {
    requestDeviceCode();
    return;
  }
  if (state_ != DeviceCodeState::Pending) return;
  if (millis() >= expiresAtMs_) {
    setState(DeviceCodeState::Timeout);
    logState("timeout");
    return;
  }
  if (millis() - lastPollMs_ < intervalMs_) return;
  pollToken();
}

uint32_t DeviceCodeFlow::expiresInSeconds() const {
  if (state_ != DeviceCodeState::Pending) return 0;
  if (millis() >= expiresAtMs_) return 0;
  return (expiresAtMs_ - millis()) / 1000;
}

void DeviceCodeFlow::wipeTokens() {
  if (idToken_ != nullptr) {
    CryptoUtil::secureWipe(idToken_, strlen(idToken_));
    free(idToken_);
    idToken_ = nullptr;
  }
  if (accessToken_ != nullptr) {
    CryptoUtil::secureWipe(accessToken_, strlen(accessToken_));
    free(accessToken_);
    accessToken_ = nullptr;
  }
}

void DeviceCodeFlow::requestDeviceCode() {
  if (WiFi.status() != WL_CONNECTED) {
    setState(DeviceCodeState::Offline);
    logState("offline");
    return;
  }

  char url[192];
  EntraConfig::authorityUrl(url, sizeof(url), "/oauth2/v2.0/devicecode");
  char scope[96];
  EntraConfig::scope(scope, sizeof(scope));
  char form[192];
  snprintf(form, sizeof(form), "client_id=%s&scope=%s", EntraConfig::clientId,
           scope);

  char* body = static_cast<char*>(malloc(EntraConfig::maxHttpBodyBytes));
  if (body == nullptr) {
    setState(DeviceCodeState::Offline);
    return;
  }
  size_t length = 0;
  int status = 0;
  const bool ok =
      https_.postForm(url, form, body, EntraConfig::maxHttpBodyBytes, &length,
                      &status);
  cJSON* document = ok ? cJSON_Parse(body) : nullptr;
  CryptoUtil::secureWipe(body, EntraConfig::maxHttpBodyBytes);
  free(body);

  if (document == nullptr) {
    setState(DeviceCodeState::Offline);
    logState("offline");
    return;
  }

  copyJsonString(document, "device_code", deviceCode_, sizeof(deviceCode_));
  copyJsonString(document, "user_code", userCode_, sizeof(userCode_));
  copyJsonString(document, "verification_uri", verificationUri_,
                 sizeof(verificationUri_));
  copyJsonString(document, "verification_uri_complete",
                 verificationUriComplete_, sizeof(verificationUriComplete_));
  copyJsonString(document, "message", message_, sizeof(message_));
  cJSON* interval = cJSON_GetObjectItemCaseSensitive(document, "interval");
  cJSON* expires = cJSON_GetObjectItemCaseSensitive(document, "expires_in");
  intervalMs_ = cJSON_IsNumber(interval)
                    ? static_cast<uint32_t>(interval->valuedouble) * 1000
                    : 5000;
  const uint32_t expiresMs =
      cJSON_IsNumber(expires)
          ? static_cast<uint32_t>(expires->valuedouble) * 1000
          : 900000;
  cJSON_Delete(document);

  if (deviceCode_[0] == '\0' || userCode_[0] == '\0') {
    setState(DeviceCodeState::Denied);
    logState("denied");
    return;
  }

  lastPollMs_ = millis();
  expiresAtMs_ = millis() + expiresMs;
  setState(DeviceCodeState::Pending);
  logState("pending user sign-in");
}

void DeviceCodeFlow::pollToken() {
  lastPollMs_ = millis();
  if (WiFi.status() != WL_CONNECTED) {
    setState(DeviceCodeState::Offline);
    logState("offline");
    return;
  }

  char url[192];
  EntraConfig::authorityUrl(url, sizeof(url), "/oauth2/v2.0/token");
  char form[768];
  snprintf(form, sizeof(form),
           "grant_type=urn:ietf:params:oauth:grant-type:device_code&"
           "client_id=%s&device_code=%s",
           EntraConfig::clientId, deviceCode_);

  char* body = static_cast<char*>(malloc(EntraConfig::maxHttpBodyBytes));
  if (body == nullptr) {
    setState(DeviceCodeState::Offline);
    return;
  }
  size_t length = 0;
  int status = 0;
  const bool ok =
      https_.postForm(url, form, body, EntraConfig::maxHttpBodyBytes, &length,
                      &status);
  cJSON* document = ok ? cJSON_Parse(body) : nullptr;
  CryptoUtil::secureWipe(body, EntraConfig::maxHttpBodyBytes);
  CryptoUtil::secureWipe(form, sizeof(form));
  free(body);

  if (document == nullptr) {
    setState(DeviceCodeState::Offline);
    logState("offline");
    return;
  }

  cJSON* error = cJSON_GetObjectItemCaseSensitive(document, "error");
  if (cJSON_IsString(error) && error->valuestring != nullptr) {
    const char* code = error->valuestring;
    if (strcmp(code, "authorization_pending") == 0) {
      logState("authorization_pending");
    } else if (strcmp(code, "slow_down") == 0) {
      intervalMs_ += 5000;
      logState("slow_down");
    } else if (strcmp(code, "expired_token") == 0) {
      setState(DeviceCodeState::Timeout);
      logState("timeout");
    } else if (strcmp(code, "access_denied") == 0 ||
               strcmp(code, "authorization_declined") == 0) {
      setState(DeviceCodeState::Denied);
      logState("denied");
    } else {
      setState(DeviceCodeState::Denied);
      logState("denied");
    }
    cJSON_Delete(document);
    return;
  }

  wipeTokens();
  idToken_ = duplicateJsonString(document, "id_token");
  accessToken_ = duplicateJsonString(document, "access_token");
  cJSON_Delete(document);
  CryptoUtil::secureWipe(deviceCode_, sizeof(deviceCode_));

  if (idToken_ == nullptr) {
    setState(DeviceCodeState::Denied);
    logState("denied");
    return;
  }
  setState(DeviceCodeState::Authorized);
  logState("token received");
}

void DeviceCodeFlow::setState(DeviceCodeState state) {
  if (state != DeviceCodeState::Pending && state != DeviceCodeState::Authorized) {
    clearSensitive();
  }
  if (state != DeviceCodeState::Authorized) wipeTokens();
  state_ = state;
}

void DeviceCodeFlow::clearSensitive() {
  CryptoUtil::secureWipe(deviceCode_, sizeof(deviceCode_));
  userCode_[0] = '\0';
  verificationUri_[0] = '\0';
  verificationUriComplete_[0] = '\0';
  message_[0] = '\0';
  intervalMs_ = 5000;
  lastPollMs_ = 0;
  expiresAtMs_ = 0;
}
