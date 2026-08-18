#pragma once

#include "EntraHttpsClient.h"

enum class DeviceCodeState {
  Idle,
  StartRequested,
  Pending,
  Authorized,
  Denied,
  Timeout,
  Offline,
  Cancelled,
};

class DeviceCodeFlow {
 public:
  void requestStart();
  void cancel();
  void reject();
  void loop();
  void resetToIdle();

  DeviceCodeState state() const { return state_; }
  const char* userCode() const { return userCode_; }
  const char* verificationUri() const { return verificationUri_; }
  const char* verificationUriComplete() const {
    return verificationUriComplete_;
  }
  const char* message() const { return message_; }
  const char* idToken() const { return idToken_; }
  const char* accessToken() const { return accessToken_; }
  uint32_t expiresInSeconds() const;
  void wipeTokens();

 private:
  void requestDeviceCode();
  void pollToken();
  void setState(DeviceCodeState state);
  void clearSensitive();

  EntraHttpsClient https_;
  DeviceCodeState state_ = DeviceCodeState::Idle;
  char deviceCode_[512] = {};
  char userCode_[32] = {};
  char verificationUri_[160] = {};
  char verificationUriComplete_[192] = {};
  char message_[192] = {};
  char* idToken_ = nullptr;
  char* accessToken_ = nullptr;
  uint32_t intervalMs_ = 5000;
  uint32_t lastPollMs_ = 0;
  uint32_t expiresAtMs_ = 0;
};
