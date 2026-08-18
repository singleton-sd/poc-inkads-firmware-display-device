#include "LocalWebServer.h"

#include <WiFi.h>
#include <esp_https_server.h>

#include "../config/DeviceConfig.h"
#include "../config/TlsCredentials.h"
#include "../platform/DeviceIdentity.h"
#include "pages/AdminPage.h"
#include "pages/HomePage.h"
#include "styles/DesignTokens.h"

void LocalWebServer::begin(const String& adminPassword) {
  adminPassword_ = adminPassword;
  httpServer_.on("/", HTTP_GET, [this]() { showHome(); });
  httpServer_.on("/admin", HTTP_GET, [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/update", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.onNotFound(
      [this]() { httpServer_.send(404, "text/plain", "Not found"); });
  httpServer_.begin();

  Serial.print("Local page: http://");
  Serial.println(WiFi.localIP());
  if (startHttpsServer()) {
    Serial.print("Secure admin page: https://");
    Serial.print(DeviceIdentity::hostname());
    Serial.println(".local/admin");
  } else {
    Serial.println("Secure admin page unavailable; admin routes fail closed.");
  }
}

void LocalWebServer::loop() { httpServer_.handleClient(); }

bool LocalWebServer::authenticate() {
  if (httpServer_.authenticate(DeviceConfig::adminUsername,
                               adminPassword_.c_str())) {
    return true;
  }
  httpServer_.requestAuthentication();
  return false;
}

void LocalWebServer::showHome() {
  String page = FPSTR(HOME_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  httpServer_.send(200, "text/html", page);
}

void LocalWebServer::showAdmin() {
  if (!authenticate()) return;
  String page = FPSTR(ADMIN_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{IP}}", WiFi.localIP().toString());
  httpServer_.send(200, "text/html", page);
}

void LocalWebServer::refuseInsecureAdmin() {
  httpServer_.sendHeader("Cache-Control", "no-store");
  httpServer_.send(426, "text/plain",
                   "Administration requires HTTPS. Open https://" +
                       DeviceIdentity::hostname() + ".local/admin");
}

bool LocalWebServer::startHttpsServer() {
  if (!TlsCredentials::configured()) {
    Serial.println(
        "TLS certificate is not provisioned. Copy "
        "TlsCredentials.local.example.h to TlsCredentials.local.h.");
    return false;
  }

  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.httpd.max_uri_handlers = 6;
  config.httpd.stack_size = 12288;
  config.servercert =
      reinterpret_cast<const uint8_t*>(TlsCredentials::certificatePem);
  config.servercert_len = strlen(TlsCredentials::certificatePem) + 1;
  config.prvtkey_pem =
      reinterpret_cast<const uint8_t*>(TlsCredentials::privateKeyPem);
  config.prvtkey_len = strlen(TlsCredentials::privateKeyPem) + 1;

  if (httpd_ssl_start(&httpsServer_, &config) != ESP_OK) return false;

  const httpd_uri_t home = {
      .uri = "/", .method = HTTP_GET, .handler = handleHttpsHome,
      .user_ctx = this};
  const httpd_uri_t admin = {
      .uri = "/admin", .method = HTTP_GET, .handler = handleHttpsAdmin,
      .user_ctx = this};
  const httpd_uri_t update = {
      .uri = "/admin/update", .method = HTTP_POST,
      .handler = handleHttpsUpdate, .user_ctx = this};

  return httpd_register_uri_handler(httpsServer_, &home) == ESP_OK &&
         httpd_register_uri_handler(httpsServer_, &admin) == ESP_OK &&
         httpd_register_uri_handler(httpsServer_, &update) == ESP_OK;
}

esp_err_t LocalWebServer::handleHttpsHome(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->sendHttpsHome(request);
}

esp_err_t LocalWebServer::handleHttpsAdmin(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->sendHttpsAdmin(request);
}

esp_err_t LocalWebServer::handleHttpsUpdate(httpd_req_t* request) {
  auto* server = static_cast<LocalWebServer*>(request->user_ctx);
  return server->otaUpdateService_.handleHttpsUpdate(request,
                                                      server->adminPassword_);
}

esp_err_t LocalWebServer::sendHttpsHome(httpd_req_t* request) {
  String page = FPSTR(HOME_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, page.c_str(), page.length());
}

esp_err_t LocalWebServer::sendHttpsAdmin(httpd_req_t* request) {
  if (!otaUpdateService_.authenticateHttps(request, adminPassword_)) {
    return ESP_OK;
  }
  String page = FPSTR(ADMIN_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{IP}}", WiFi.localIP().toString());
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, page.c_str(), page.length());
}
