#include "LocalWebServer.h"

#include <cstring>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_https_server.h>

#include "../config/DeviceConfig.h"
#include "../config/EntraConfig.h"
#include "../config/TlsCredentials.h"
#include "../platform/DeviceIdentity.h"
#include "pages/AdminPage.h"
#include "pages/HomePage.h"
#include "pages/SignInPage.h"
#include "styles/DesignTokens.h"

void LocalWebServer::begin() {
  httpServer_.on("/", HTTP_GET, [this]() { showHome(); });
  httpServer_.on("/admin", HTTP_GET, [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin", HTTP_POST, [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/session", HTTP_GET, [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/session", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/session/cancel", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/logout", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
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
    Serial.print("[entra] https ready free_heap=");
    Serial.println(ESP.getFreeHeap());
  } else {
    Serial.println("Secure admin page unavailable; admin routes fail closed.");
  }
}

void LocalWebServer::loop() {
  httpServer_.handleClient();
  entraAuth_.loop();
}

void LocalWebServer::showHome() {
  String page = FPSTR(HOME_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  httpServer_.send(200, "text/html", page);
}

void LocalWebServer::refuseInsecureAdmin() {
  httpServer_.sendHeader("Cache-Control", "no-store");
  httpServer_.send(426, "text/plain",
                   "Administration requires HTTPS. Open https://" +
                       DeviceIdentity::hostname() + ".local/admin");
}

bool LocalWebServer::registerGet(const char* uri,
                                 esp_err_t (*handler)(httpd_req_t*)) {
  const httpd_uri_t route = {
      .uri = uri, .method = HTTP_GET, .handler = handler, .user_ctx = this};
  return httpd_register_uri_handler(httpsServer_, &route) == ESP_OK;
}

bool LocalWebServer::registerPost(const char* uri,
                                  esp_err_t (*handler)(httpd_req_t*)) {
  const httpd_uri_t route = {
      .uri = uri, .method = HTTP_POST, .handler = handler, .user_ctx = this};
  return httpd_register_uri_handler(httpsServer_, &route) == ESP_OK;
}

bool LocalWebServer::startHttpsServer() {
  if (!TlsCredentials::configured()) {
    Serial.println(
        "TLS certificate is not provisioned. Copy "
        "TlsCredentials.local.example.h to TlsCredentials.local.h.");
    return false;
  }

  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.httpd.max_uri_handlers = 10;
  config.httpd.stack_size = 16384;
  config.servercert =
      reinterpret_cast<const uint8_t*>(TlsCredentials::certificatePem);
  config.servercert_len = strlen(TlsCredentials::certificatePem) + 1;
  config.prvtkey_pem =
      reinterpret_cast<const uint8_t*>(TlsCredentials::privateKeyPem);
  config.prvtkey_len = strlen(TlsCredentials::privateKeyPem) + 1;

  if (httpd_ssl_start(&httpsServer_, &config) != ESP_OK) return false;

  return registerGet("/", handleHttpsHome) &&
         registerGet("/admin", handleHttpsAdmin) &&
         registerGet("/admin/session", handleHttpsSessionGet) &&
         registerPost("/admin/session", handleHttpsSessionPost) &&
         registerPost("/admin/session/cancel", handleHttpsSessionCancel) &&
         registerPost("/admin/logout", handleHttpsLogout) &&
         registerPost("/admin/update", handleHttpsUpdate);
}

esp_err_t LocalWebServer::handleHttpsHome(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->sendHttpsHome(request);
}

esp_err_t LocalWebServer::handleHttpsAdmin(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->sendHttpsAdmin(request);
}

esp_err_t LocalWebServer::handleHttpsSessionGet(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)
      ->sendHttpsSession(request);
}

esp_err_t LocalWebServer::handleHttpsSessionPost(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)
      ->startHttpsSession(request);
}

esp_err_t LocalWebServer::handleHttpsSessionCancel(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)
      ->cancelHttpsSession(request);
}

esp_err_t LocalWebServer::handleHttpsLogout(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->logoutHttps(request);
}

esp_err_t LocalWebServer::handleHttpsUpdate(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->updateHttps(request);
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
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  if (entraAuth_.sessions().matchesSessionCookie(request) &&
      entraAuth_.hasAdminSession()) {
    entraAuth_.sessions().touch();
    String page = FPSTR(ADMIN_PAGE);
    page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
    page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
    page.replace("{{IP}}", WiFi.localIP().toString());
    page.replace("{{CSRF}}", entraAuth_.sessions().csrfHex());
    entraAuth_.sessions().applySessionCookie(request);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_send(request, page.c_str(), page.length());
  }

  entraAuth_.sessions().ensureLoginCsrf();
  String page = FPSTR(SIGN_IN_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{LOGIN_CSRF}}", entraAuth_.sessions().loginCsrf());
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, page.c_str(), page.length());
}

esp_err_t LocalWebServer::sendHttpsSession(httpd_req_t* request) {
  cJSON* json = cJSON_CreateObject();
  const char* state = "idle";
  if (entraAuth_.hasAdminSession()) {
    state = "authorized";
    entraAuth_.sessions().touch();
    entraAuth_.sessions().applySessionCookie(request);
  } else if (!EntraConfig::configured()) {
    state = "misconfigured";
  } else {
    switch (entraAuth_.flow().state()) {
      case DeviceCodeState::StartRequested:
      case DeviceCodeState::Pending:
        state = "pending";
        break;
      case DeviceCodeState::Authorized:
        state = "authorized";
        break;
      case DeviceCodeState::Denied:
        state = "denied";
        break;
      case DeviceCodeState::Timeout:
        state = "timeout";
        break;
      case DeviceCodeState::Offline:
        state = "offline";
        break;
      case DeviceCodeState::Cancelled:
        state = "cancelled";
        break;
      case DeviceCodeState::Idle:
        state = "idle";
        break;
    }
  }
  cJSON_AddStringToObject(json, "state", state);
  if (strcmp(state, "pending") == 0) {
    cJSON_AddStringToObject(json, "user_code", entraAuth_.flow().userCode());
    cJSON_AddStringToObject(json, "verification_uri",
                            entraAuth_.flow().verificationUri());
    cJSON_AddStringToObject(json, "verification_uri_complete",
                            entraAuth_.flow().verificationUriComplete());
    cJSON_AddStringToObject(json, "message", entraAuth_.flow().message());
    cJSON_AddNumberToObject(json, "expires_in",
                            entraAuth_.flow().expiresInSeconds());
  }
  char* body = cJSON_PrintUnformatted(json);
  cJSON_Delete(json);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_type(request, "application/json");
  const esp_err_t result =
      httpd_resp_send(request, body == nullptr ? "{}" : body,
                      body == nullptr ? 2 : strlen(body));
  if (body != nullptr) cJSON_free(body);
  return result;
}

esp_err_t LocalWebServer::startHttpsSession(httpd_req_t* request) {
  drainBody(request);
  if (entraAuth_.hasAdminSession() &&
      entraAuth_.sessions().matchesSessionCookie(request)) {
    return sendHttpsSession(request);
  }
  if (!requireLoginCsrf(request)) return ESP_OK;
  if (!EntraConfig::configured()) {
    return sendText(request, "503 Service Unavailable", "application/json",
                    "{\"state\":\"misconfigured\"}");
  }
  entraAuth_.startSignIn();
  return sendHttpsSession(request);
}

esp_err_t LocalWebServer::cancelHttpsSession(httpd_req_t* request) {
  drainBody(request);
  if (!requireLoginCsrf(request)) return ESP_OK;
  entraAuth_.cancelSignIn();
  return sendHttpsSession(request);
}

esp_err_t LocalWebServer::logoutHttps(httpd_req_t* request) {
  drainBody(request);
  if (!requireSessionCsrf(request)) return ESP_OK;
  entraAuth_.signOut();
  entraAuth_.sessions().applyExpiredSessionCookie(request);
  return sendText(request, "200 OK", "application/json",
                  "{\"state\":\"idle\"}");
}

esp_err_t LocalWebServer::updateHttps(httpd_req_t* request) {
  if (!requireSessionCsrf(request)) return ESP_OK;
  entraAuth_.sessions().touch();
  return otaUpdateService_.handleHttpsUpdate(request);
}

bool LocalWebServer::requireSession(httpd_req_t* request) {
  if (entraAuth_.sessions().matchesSessionCookie(request) &&
      entraAuth_.hasAdminSession()) {
    return true;
  }
  sendText(request, "401 Unauthorized", "text/plain",
           "Session expired. Sign in again.");
  return false;
}

bool LocalWebServer::requireSessionCsrf(httpd_req_t* request) {
  if (!requireSession(request)) return false;
  if (entraAuth_.sessions().matchesCsrfHeader(request)) return true;
  sendText(request, "403 Forbidden", "text/plain", "CSRF token was rejected.");
  return false;
}

bool LocalWebServer::requireLoginCsrf(httpd_req_t* request) {
  if (entraAuth_.sessions().matchesLoginCsrf(request)) return true;
  sendText(request, "403 Forbidden", "text/plain", "CSRF token was rejected.");
  return false;
}

void LocalWebServer::drainBody(httpd_req_t* request) {
  char sink[64];
  int remaining = request->content_len;
  while (remaining > 0) {
    const int wanted = remaining > static_cast<int>(sizeof(sink))
                           ? static_cast<int>(sizeof(sink))
                           : remaining;
    const int received = httpd_req_recv(request, sink, wanted);
    if (received <= 0) break;
    remaining -= received;
  }
}

esp_err_t LocalWebServer::sendText(httpd_req_t* request, const char* status,
                                   const char* type, const char* body) {
  httpd_resp_set_status(request, status);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_type(request, type);
  return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}
