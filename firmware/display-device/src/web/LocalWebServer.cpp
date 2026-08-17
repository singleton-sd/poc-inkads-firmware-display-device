#include "LocalWebServer.h"

#include <WiFi.h>

#include "../config/DeviceConfig.h"
#include "../platform/DeviceIdentity.h"
#include "pages/AdminPage.h"
#include "pages/HomePage.h"
#include "styles/DesignTokens.h"

void LocalWebServer::begin(const String& adminPassword) {
  adminPassword_ = adminPassword;
  server_.on("/", HTTP_GET, [this]() { showHome(); });
  server_.on("/admin", HTTP_GET, [this]() { showAdmin(); });
  otaUpdateService_.registerRoutes(server_, adminPassword_);
  server_.onNotFound(
      [this]() { server_.send(404, "text/plain", "Not found"); });
  server_.begin();

  Serial.print("Local page: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Admin page: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/admin");
}

void LocalWebServer::loop() { server_.handleClient(); }

bool LocalWebServer::authenticate() {
  if (server_.authenticate(DeviceConfig::adminUsername,
                           adminPassword_.c_str())) {
    return true;
  }
  server_.requestAuthentication();
  return false;
}

void LocalWebServer::showHome() {
  String page = FPSTR(HOME_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  server_.send(200, "text/html", page);
}

void LocalWebServer::showAdmin() {
  if (!authenticate()) return;
  String page = FPSTR(ADMIN_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{VERSION}}", DeviceConfig::firmwareVersion);
  page.replace("{{IP}}", WiFi.localIP().toString());
  server_.send(200, "text/html", page);
}
