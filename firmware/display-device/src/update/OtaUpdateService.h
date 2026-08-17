#pragma once

#include <Arduino.h>
#include <WebServer.h>

class OtaUpdateService {
 public:
  void registerRoutes(WebServer& server, const String& adminPassword);

 private:
  bool authenticate();
  void receiveUpdate();
  void finishUpdate();

  WebServer* server_ = nullptr;
  String adminPassword_;
  bool updateStarted_ = false;
  bool updateSucceeded_ = false;
};
