#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

class AdminSessionStore {
 public:
  void ensureLoginCsrf();
  const char* loginCsrf() const { return loginCsrf_; }
  bool create();
  void clear();
  bool valid() const;
  void touch();
  const char* sessionIdHex() const { return sessionId_; }
  const char* csrfHex() const { return csrf_; }
  bool matchesSessionCookie(httpd_req_t* request) const;
  bool matchesCsrfHeader(httpd_req_t* request) const;
  bool matchesLoginCsrf(httpd_req_t* request);
  void applySessionCookie(httpd_req_t* request) const;
  void applyExpiredSessionCookie(httpd_req_t* request) const;

 private:
  bool readHeader(httpd_req_t* request, const char* name, char* out,
                  size_t outSize) const;
  bool cookieValue(httpd_req_t* request, const char* name, char* out,
                   size_t outSize) const;

  bool active_ = false;
  char sessionId_[33] = {};
  char csrf_[33] = {};
  char loginCsrf_[33] = {};
  uint32_t createdMs_ = 0;
  uint32_t lastSeenMs_ = 0;
  mutable char cookieHeader_[160] = {};
};
