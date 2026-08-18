#pragma once

#include <WebServer.h>
#include <esp_http_server.h>

#include "../auth/EntraAuthService.h"
#include "../config/ConfigStore.h"
#include "../config/TlsCertificateStore.h"
#include "../update/OtaUpdateService.h"

class LocalWebServer {
 public:
  explicit LocalWebServer(ConfigStore& configStore);
  void begin();
  void loop();

 private:
  void showHome();
  void refuseInsecureAdmin();
  bool startHttpsServer();
  static esp_err_t handleHttpsHome(httpd_req_t* request);
  static esp_err_t handleHttpsAdmin(httpd_req_t* request);
  static esp_err_t handleHttpsSessionGet(httpd_req_t* request);
  static esp_err_t handleHttpsSessionPost(httpd_req_t* request);
  static esp_err_t handleHttpsSessionCancel(httpd_req_t* request);
  static esp_err_t handleHttpsLogout(httpd_req_t* request);
  static esp_err_t handleHttpsTlsCertificate(httpd_req_t* request);
  static esp_err_t handleHttpsUpdate(httpd_req_t* request);
  static esp_err_t handleHttpsNetworks(httpd_req_t* request);
  static esp_err_t handleHttpsWifi(httpd_req_t* request);
  static esp_err_t handleHttpsReset(httpd_req_t* request);
  esp_err_t sendHttpsHome(httpd_req_t* request);
  esp_err_t sendHttpsAdmin(httpd_req_t* request);
  esp_err_t sendHttpsSession(httpd_req_t* request);
  esp_err_t startHttpsSession(httpd_req_t* request);
  esp_err_t cancelHttpsSession(httpd_req_t* request);
  esp_err_t logoutHttps(httpd_req_t* request);
  esp_err_t updateTlsCertificate(httpd_req_t* request);
  esp_err_t updateHttps(httpd_req_t* request);
  esp_err_t sendHttpsNetworks(httpd_req_t* request);
  esp_err_t updateWifiHttps(httpd_req_t* request);
  esp_err_t resetHttps(httpd_req_t* request);
  bool requireSession(httpd_req_t* request);
  bool requireSessionCsrf(httpd_req_t* request);
  bool requireLoginCsrf(httpd_req_t* request);
  void drainBody(httpd_req_t* request);
  esp_err_t sendText(httpd_req_t* request, const char* status, const char* type,
                     const char* body);
  bool registerGet(const char* uri, esp_err_t (*handler)(httpd_req_t*));
  bool registerPost(const char* uri, esp_err_t (*handler)(httpd_req_t*));

  ConfigStore& configStore_;
  WebServer httpServer_{80};
  httpd_handle_t httpsServer_ = nullptr;
  OtaUpdateService otaUpdateService_;
  EntraAuthService entraAuth_;
  TlsCertificateStore tlsStore_;
};