#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_ENTRA

#include "EntraAuthService.h"

#include "../config/DeviceConfig.h"
#include "../config/EntraConfig.h"

void EntraAuthService::loop() {
  if (!sessions_.valid()) sessions_.clear();
  flow_.loop();
  if (flow_.state() == DeviceCodeState::Authorized && !authorizationHandled_) {
    finishAuthorization();
  }
  if (flow_.state() != DeviceCodeState::Authorized) {
    authorizationHandled_ = false;
  }
}

void EntraAuthService::startSignIn() {
  if (!EntraConfig::configured()) return;
  authorizationHandled_ = false;
  flow_.requestStart();
}

void EntraAuthService::cancelSignIn() {
  flow_.cancel();
  authorizationHandled_ = false;
}

void EntraAuthService::signOut() {
  sessions_.clear();
  flow_.resetToIdle();
  authorizationHandled_ = false;
}

void EntraAuthService::resetSignIn() {
  flow_.resetToIdle();
  authorizationHandled_ = false;
}

void EntraAuthService::finishAuthorization() {
  authorizationHandled_ = true;
  const bool allowed =
      validator_.authorize(flow_.idToken(), flow_.accessToken());
  flow_.wipeTokens();
  if (!allowed) {
    flow_.reject();
    if (DeviceConfig::debugLogging) {
      Serial.println("[entra] token rejected");
    }
    return;
  }
  if (!sessions_.create()) {
    flow_.resetToIdle();
    return;
  }
  flow_.resetToIdle();
  if (DeviceConfig::debugLogging) {
    Serial.print("[entra] authorized free_heap=");
    Serial.println(ESP.getFreeHeap());
  }
}

#endif
