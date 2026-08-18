#include "ProvisioningPortal.h"

#include <WiFi.h>

#include "../config/DeviceConfig.h"
#include "../config/DeviceSettings.h"
#include "../platform/DeviceIdentity.h"
#include "../platform/DebugLog.h"
#include "WifiScan.h"
#include "../web/pages/ProvisioningPage.h"
#include "../web/styles/DesignTokens.h"

namespace {
constexpr uint16_t dnsPort = 53;
}

ProvisioningPortal::ProvisioningPortal(ConfigStore& configStore)
    : configStore_(configStore) {}

bool ProvisioningPortal::begin() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_AP_STA);

  accessPointName_ = buildAccessPointName();
  if (!WiFi.softAP(accessPointName_.c_str())) {
    return false;
  }

  dnsServer_.start(dnsPort, "*", WiFi.softAPIP());

  webServer_.on("/", HTTP_GET, [this]() { showPortal(); });
  webServer_.on("/networks", HTTP_GET, [this]() { showNetworks(); });
  webServer_.on("/save", HTTP_POST, [this]() { saveCredentials(); });

  // Common captive-portal probes used by Android, Apple, and Windows.
  webServer_.on("/generate_204", HTTP_ANY, [this]() { redirectToPortal(); });
  webServer_.on("/hotspot-detect.html", HTTP_ANY,
                [this]() { redirectToPortal(); });
  webServer_.on("/connecttest.txt", HTTP_ANY,
                [this]() { redirectToPortal(); });
  webServer_.onNotFound([this]() { redirectToPortal(); });
  webServer_.begin();

  Serial.print("Provisioning access point: ");
  Serial.println(accessPointName_);
  Serial.print("Setup address: http://");
  Serial.println(WiFi.softAPIP());
  return true;
}

void ProvisioningPortal::loop() {
  dnsServer_.processNextRequest();
  webServer_.handleClient();
}

const String& ProvisioningPortal::accessPointName() const {
  return accessPointName_;
}

void ProvisioningPortal::showPortal() {
  String page = FPSTR(PROVISIONING_PAGE);
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  webServer_.send(200, "text/html", page);
}

void ProvisioningPortal::showNetworks() { WifiScan::sendJson(webServer_); }

void ProvisioningPortal::saveCredentials() {
  DeviceSettings credentials;
  credentials.ssid = webServer_.arg("ssid");
  credentials.password = webServer_.arg("password");
  credentials.ssid.trim();
  DebugLog::credentials("received from provisioning form", credentials);

  if (!configStore_.save(credentials)) {
    webServer_.send(400, "text/plain",
                    "Invalid configuration. Check the field lengths.");
    return;
  }

  String page = R"html(
<!doctype html><html lang="en" data-theme="light"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>InkAds configuration saved</title><style>{{DESIGN_TOKENS}}
*{box-sizing:border-box}body{margin:0;background:var(--ssd-color-background-muted);
color:var(--ssd-color-gray-900);font-family:var(--ssd-font-family-body)}main{
max-width:30rem;margin:10vh auto;padding:var(--ssd-space-300)}section{background:
var(--ssd-color-background-default);border:1px solid var(--ssd-color-border-default);
border-radius:var(--ssd-radius-xl);padding:var(--ssd-space-400);text-align:center}
.count{font-size:var(--ssd-font-size-400);font-weight:var(--ssd-font-weight-bold);
color:var(--ssd-color-text-default)}a{color:var(--ssd-color-text-link)}</style></head>
<body><main><section><h1>Configuration saved</h1><p>The InkAds device is restarting.</p>
<p>Opening <a id="destination" href="{{LOCAL_URL}}">{{HOSTNAME}}.local</a> in</p>
<p class="count"><span id="seconds">15</span> seconds</p>
<p><small>Keep this page open while the device reconnects.</small></p></section></main>
<script>let remaining=15;const output=document.querySelector('#seconds');
const timer=setInterval(()=>{remaining-=1;output.textContent=remaining;if(remaining<=0){
clearInterval(timer);location.href='{{LOCAL_URL}}'}},1000);</script></body></html>
)html";
  page.replace("{{DESIGN_TOKENS}}", FPSTR(DESIGN_TOKENS_CSS));
  page.replace("{{LOCAL_URL}}", DeviceIdentity::localUrl());
  page.replace("{{HOSTNAME}}", DeviceIdentity::hostname());
  webServer_.send(200, "text/html", page);
  delay(DeviceConfig::restartDelayMs);
  ESP.restart();
}

void ProvisioningPortal::redirectToPortal() {
  webServer_.sendHeader("Location", String("http://") + WiFi.softAPIP() + "/",
                        true);
  webServer_.send(302, "text/plain", "");
}

String ProvisioningPortal::buildAccessPointName() const {
  return DeviceIdentity::setupAccessPointName();
}
