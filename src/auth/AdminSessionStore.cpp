#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_ENTRA

#include "AdminSessionStore.h"

#include "../config/EntraConfig.h"
#include "CryptoUtil.h"

void AdminSessionStore::ensureLoginCsrf() {
  if (loginCsrf_[0] == '\0') CryptoUtil::randomHex(loginCsrf_, 16);
}

bool AdminSessionStore::create() {
  CryptoUtil::randomHex(sessionId_, 16);
  CryptoUtil::randomHex(csrf_, 16);
  createdMs_ = millis();
  lastSeenMs_ = createdMs_;
  active_ = sessionId_[0] != '\0' && csrf_[0] != '\0';
  return active_;
}

void AdminSessionStore::clear() {
  active_ = false;
  createdMs_ = 0;
  lastSeenMs_ = 0;
  CryptoUtil::secureWipe(sessionId_, sizeof(sessionId_));
  CryptoUtil::secureWipe(csrf_, sizeof(csrf_));
}

bool AdminSessionStore::valid() const {
  if (!active_) return false;
  const uint32_t now = millis();
  if (now - createdMs_ > EntraConfig::sessionAbsoluteTimeoutMs) return false;
  if (now - lastSeenMs_ > EntraConfig::sessionInactivityTimeoutMs) return false;
  return true;
}

void AdminSessionStore::touch() {
  if (valid()) lastSeenMs_ = millis();
}

bool AdminSessionStore::matchesSessionCookie(httpd_req_t* request) const {
  if (!valid()) return false;
  char value[33] = {};
  if (!cookieValue(request, "inkads_session", value, sizeof(value))) {
    return false;
  }
  return CryptoUtil::constantTimeEquals(value, sessionId_);
}

bool AdminSessionStore::matchesCsrfHeader(httpd_req_t* request) const {
  if (!valid()) return false;
  char header[33] = {};
  if (!readHeader(request, "X-CSRF-Token", header, sizeof(header))) {
    return false;
  }
  return CryptoUtil::constantTimeEquals(header, csrf_);
}

bool AdminSessionStore::matchesLoginCsrf(httpd_req_t* request) {
  ensureLoginCsrf();
  char header[33] = {};
  if (!readHeader(request, "X-CSRF-Token", header, sizeof(header))) {
    return false;
  }
  return CryptoUtil::constantTimeEquals(header, loginCsrf_);
}

void AdminSessionStore::applySessionCookie(httpd_req_t* request) const {
  snprintf(cookieHeader_, sizeof(cookieHeader_),
           "inkads_session=%s; Path=/; HttpOnly; Secure; SameSite=Strict",
           sessionId_);
  httpd_resp_set_hdr(request, "Set-Cookie", cookieHeader_);
}

void AdminSessionStore::applyExpiredSessionCookie(httpd_req_t* request) const {
  snprintf(cookieHeader_, sizeof(cookieHeader_),
           "inkads_session=; Path=/; HttpOnly; Secure; SameSite=Strict; "
           "Max-Age=0");
  httpd_resp_set_hdr(request, "Set-Cookie", cookieHeader_);
}

bool AdminSessionStore::readHeader(httpd_req_t* request, const char* name,
                                   char* out, size_t outSize) const {
  const size_t length = httpd_req_get_hdr_value_len(request, name);
  if (length == 0 || length >= outSize) return false;
  return httpd_req_get_hdr_value_str(request, name, out, outSize) == ESP_OK;
}

bool AdminSessionStore::cookieValue(httpd_req_t* request, const char* name,
                                    char* out, size_t outSize) const {
  char header[256] = {};
  if (!readHeader(request, "Cookie", header, sizeof(header))) return false;

  const String needle = String(name) + "=";
  const char* found = strstr(header, needle.c_str());
  if (found == nullptr) return false;
  found += needle.length();
  size_t index = 0;
  while (found[index] != '\0' && found[index] != ';' && index + 1 < outSize) {
    out[index] = found[index];
    index += 1;
  }
  out[index] = '\0';
  return index > 0;
}

#endif
