#include "LocalWebServer.h"

#include <cstring>
#include <memory>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_https_server.h>

#include "../config/DeviceConfig.h"
#include "../config/DeviceSettings.h"
#include "../config/EntraConfig.h"
#include "../config/TlsCredentials.h"
#include "../network/WifiScan.h"
#include "../platform/DeviceIdentity.h"
#include "pages/AdminPage.h"
#include "pages/HomePage.h"
#include "pages/SignInPage.h"
#include "styles/DesignTokens.h"

namespace {
String urlDecode(const char* encoded) {
  String decoded;
  if (encoded == nullptr) return decoded;

  for (size_t index = 0; encoded[index] != '\0'; ++index) {
    const char character = encoded[index];
    if (character == '+') {
      decoded += ' ';
      continue;
    }
    if (character == '%' && isxdigit(encoded[index + 1]) &&
        isxdigit(encoded[index + 2])) {
      char value[3] = {encoded[index + 1], encoded[index + 2], '\0'};
      decoded += static_cast<char>(strtol(value, nullptr, 16));
      index += 2;
      continue;
    }
    decoded += character;
  }

  return decoded;
}

bool readRequestBody(httpd_req_t* request, String& body) {
  body = "";
  if (request->content_len <= 0) return true;

  body.reserve(request->content_len);
  char chunk[128];
  int remaining = request->content_len;
  while (remaining > 0) {
    const int wanted = remaining > static_cast<int>(sizeof(chunk))
                           ? static_cast<int>(sizeof(chunk))
                           : remaining;
    const int received = httpd_req_recv(request, chunk, wanted);
    if (received <= 0) return false;
    body.concat(chunk, received);
    remaining -= received;
  }
  return true;
}

String readFormValue(const String& body, const char* key) {
  if (body.isEmpty()) return "";

  std::unique_ptr<char[]> form(new char[body.length() + 1]);
  memcpy(form.get(), body.c_str(), body.length() + 1);

  char value[128] = {};
  if (httpd_query_key_value(form.get(), key, value, sizeof(value)) != ESP_OK) {
    return "";
  }
  return urlDecode(value);
}
}  // namespace

LocalWebServer::LocalWebServer(ConfigStore& configStore)
    : configStore_(configStore) {}

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
  httpServer_.on("/admin/tls-certificate", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/update", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/networks", HTTP_GET, [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/wifi", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.on("/admin/reset", HTTP_POST,
                 [this]() { refuseInsecureAdmin(); });
  httpServer_.onNotFound(
      [this]() { httpServer_.send(404, "text/plain", "Not found"); });
  httpServer_.begin();

  Serial.print("Local page: http://");
  Serial.println(WiFi.localIP());
  if (startHttpsServer()) {
    Serial.print("Secure admin page: https://");
    Serial.println(DeviceIdentity::dnsName() + "/admin");
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
  page.replace("{{ADMIN_URL}}", DeviceIdentity::httpsAdminUrl());
  httpServer_.send(200, "text/html", page);
}

void LocalWebServer::refuseInsecureAdmin() {
  httpServer_.sendHeader("Cache-Control", "no-store");
  httpServer_.send(426, "text/plain",
                   "Administration requires HTTPS. Open https://" +
                       DeviceIdentity::dnsName() + "/admin");
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
  const uint32_t startMs = millis();
  const uint32_t heapBefore = ESP.getFreeHeap();
  if (!tlsStore_.begin()) {
    Serial.println("TLS storage mount failed.");
    return false;
  }
  String tlsError;
  if (!tlsStore_.loadActive(tlsError) || !tlsStore_.configured()) {
    if (TlsCredentials::configured()) {
      Serial.println("Bootstrapping TLS certificate from local header.");
      if (!tlsStore_.stage(TlsCredentials::certificatePem, TlsCredentials::privateKeyPem,
                           DeviceIdentity::dnsName(), tlsError) ||
          !tlsStore_.configured()) {
        Serial.print("TLS bootstrap failed: ");
        Serial.println(tlsError);
        return false;
      }
    } else {
      Serial.print("TLS certificate is not provisioned in flash: ");
      Serial.println(tlsError);
      return false;
    }
  }

  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.httpd.max_uri_handlers = 16;
  config.httpd.stack_size = 16384;
  config.servercert =
      reinterpret_cast<const uint8_t*>(tlsStore_.certificatePem());
  config.servercert_len = strlen(tlsStore_.certificatePem()) + 1;
  config.prvtkey_pem =
      reinterpret_cast<const uint8_t*>(tlsStore_.privateKeyPem());
  config.prvtkey_len = strlen(tlsStore_.privateKeyPem()) + 1;

  if (httpd_ssl_start(&httpsServer_, &config) != ESP_OK) return false;
  const uint32_t heapAfter = ESP.getFreeHeap();
  Serial.print("[https] start_ms=");
  Serial.print(millis() - startMs);
  Serial.print(" heap_before=");
  Serial.print(heapBefore);
  Serial.print(" heap_after=");
  Serial.println(heapAfter);

  return registerGet("/", handleHttpsHome) &&
         registerGet("/admin", handleHttpsAdmin) &&
         registerGet("/admin/session", handleHttpsSessionGet) &&
         registerPost("/admin/session", handleHttpsSessionPost) &&
         registerPost("/admin/session/cancel", handleHttpsSessionCancel) &&
         registerPost("/admin/logout", handleHttpsLogout) &&
         registerPost("/admin/tls-certificate", handleHttpsTlsCertificate) &&
         registerPost("/admin/update", handleHttpsUpdate) &&
         registerGet("/networks", handleHttpsNetworks) &&
         registerPost("/admin/wifi", handleHttpsWifi) &&
         registerPost("/admin/reset", handleHttpsReset);
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

esp_err_t LocalWebServer::handleHttpsTlsCertificate(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)
      ->updateTlsCertificate(request);
}

esp_err_t LocalWebServer::handleHttpsNetworks(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)
      ->sendHttpsNetworks(request);
}

esp_err_t LocalWebServer::handleHttpsWifi(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->updateWifiHttps(request);
}

esp_err_t LocalWebServer::handleHttpsReset(httpd_req_t* request) {
  return static_cast<LocalWebServer*>(request->user_ctx)->resetHttps(request);
}

esp_err_t LocalWebServer::sendHttpsHome(httpd_req_t* request) {
  String page = FPSTR(HOME_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  page.replace("{{ADMIN_URL}}", DeviceIdentity::httpsAdminUrl());
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
    const String connectedSsid = WiFi.SSID();
    page.replace("{{SSID}}", connectedSsid.isEmpty() ? "(unknown)" : connectedSsid);
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
  entraAuth_.sessions().applySessionCookie(request);
  return otaUpdateService_.handleHttpsUpdate(request);
}

esp_err_t LocalWebServer::updateTlsCertificate(httpd_req_t* request) {
  if (!requireSessionCsrf(request)) return ESP_OK;

  if (request->content_len <= 0 || request->content_len > 20000) {
    return sendText(request, "400 Bad Request", "text/plain",
                    "Certificate payload is empty or too large.");
  }

  String body;
  body.reserve(request->content_len);
  int remaining = request->content_len;
  char chunk[1024];
  while (remaining > 0) {
    const int wanted = remaining > static_cast<int>(sizeof(chunk))
                           ? static_cast<int>(sizeof(chunk))
                           : remaining;
    const int received = httpd_req_recv(request, chunk, wanted);
    if (received <= 0) {
      return sendText(request, "400 Bad Request", "text/plain",
                      "Failed to read certificate payload.");
    }
    body.concat(chunk, received);
    remaining -= received;
  }

  cJSON* json = cJSON_Parse(body.c_str());
  if (json == nullptr) {
    return sendText(request, "400 Bad Request", "text/plain",
                    "Certificate payload must be valid JSON.");
  }
  const cJSON* cert = cJSON_GetObjectItemCaseSensitive(json, "certificate_pem");
  const cJSON* key = cJSON_GetObjectItemCaseSensitive(json, "private_key_pem");
  if (!cJSON_IsString(cert) || !cJSON_IsString(key) || cert->valuestring == nullptr ||
      key->valuestring == nullptr) {
    cJSON_Delete(json);
    return sendText(request, "400 Bad Request", "text/plain",
                    "JSON must include certificate_pem and private_key_pem.");
  }

  String tlsError;
  const bool staged =
      tlsStore_.stage(cert->valuestring, key->valuestring, DeviceIdentity::dnsName(),
                      tlsError);
  cJSON_Delete(json);
  if (!staged) {
    const String message = "Certificate rejected: " + tlsError;
    return sendText(request, "400 Bad Request", "text/plain",
                    message.c_str());
  }

  return sendText(request, "200 OK", "text/plain",
                  "Certificate stored and activated. Reboot to apply HTTPS.");
}

esp_err_t LocalWebServer::sendHttpsNetworks(httpd_req_t* request) {
  if (!requireSession(request)) return ESP_OK;
  entraAuth_.sessions().touch();
  entraAuth_.sessions().applySessionCookie(request);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_type(request, "application/json");
  const String body = WifiScan::toJson();
  return httpd_resp_send(request, body.c_str(), body.length());
}

esp_err_t LocalWebServer::updateWifiHttps(httpd_req_t* request) {
  if (!requireSessionCsrf(request)) return ESP_OK;

  String body;
  if (!readRequestBody(request, body)) {
    return sendText(request, "400 Bad Request", "text/plain",
                    "Could not read the Wi-Fi form.");
  }

  DeviceSettings settings = configStore_.load();
  settings.ssid = readFormValue(body, "ssid");
  settings.password = readFormValue(body, "password");
  settings.ssid.trim();

  if (!configStore_.save(settings)) {
    return sendText(request, "400 Bad Request", "text/plain",
                    "Invalid Wi-Fi settings. Check the network name.");
  }

  entraAuth_.sessions().touch();
  entraAuth_.sessions().applySessionCookie(request);
  sendText(request, "200 OK", "text/plain",
           "Wi-Fi saved. The device is restarting...");
  delay(DeviceConfig::restartDelayMs);
  ESP.restart();
  return ESP_OK;
}

esp_err_t LocalWebServer::resetHttps(httpd_req_t* request) {
  if (!requireSessionCsrf(request)) return ESP_OK;
  drainBody(request);

  if (!configStore_.clear()) {
    return sendText(request, "500 Internal Server Error", "text/plain",
                    "Could not erase saved data. The device was not changed.");
  }

  entraAuth_.signOut();
  entraAuth_.sessions().applyExpiredSessionCookie(request);
  sendText(request, "200 OK", "text/plain",
           "Saved data erased. The device is restarting...");
  delay(DeviceConfig::restartDelayMs);
  ESP.restart();
  return ESP_OK;
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