#pragma once

#include "AdminSessionStore.h"
#include "DeviceCodeFlow.h"
#include "IdTokenValidator.h"

class EntraAuthService {
 public:
  void loop();
  void startSignIn();
  void cancelSignIn();
  void signOut();
  void resetSignIn();

  DeviceCodeFlow& flow() { return flow_; }
  AdminSessionStore& sessions() { return sessions_; }
  bool hasAdminSession() const { return sessions_.valid(); }

 private:
  void finishAuthorization();

  DeviceCodeFlow flow_;
  IdTokenValidator validator_;
  AdminSessionStore sessions_;
  bool authorizationHandled_ = false;
};
