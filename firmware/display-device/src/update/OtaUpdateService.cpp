#include "OtaUpdateService.h"

#include <Update.h>

#include "../config/DeviceConfig.h"

void OtaUpdateService::registerRoutes(WebServer& server,
                                      const String& adminPassword) {
  server_ = &server;
  adminPassword_ = adminPassword;
  server_->on(
      "/admin/update", HTTP_POST, [this]() { finishUpdate(); },
      [this]() { receiveUpdate(); });
}

bool OtaUpdateService::authenticate() {
  if (server_->authenticate(DeviceConfig::adminUsername,
                            adminPassword_.c_str())) {
    return true;
  }
  server_->requestAuthentication();
  return false;
}

void OtaUpdateService::receiveUpdate() {
  if (!authenticate()) return;
  HTTPUpload& upload = server_->upload();

  if (upload.status == UPLOAD_FILE_START) {
    updateStarted_ = true;
    updateSucceeded_ = false;
    if (!upload.filename.endsWith(".bin") || !Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE && !Update.hasError()) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END && !Update.hasError()) {
    updateSucceeded_ = Update.end(true);
    if (!updateSucceeded_) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void OtaUpdateService::finishUpdate() {
  if (!authenticate()) return;
  if (!updateStarted_ || !updateSucceeded_ || Update.hasError()) {
    server_->send(500, "text/plain",
                  "Firmware update failed. Check Serial output.");
    return;
  }

  server_->send(200, "text/plain", "Update installed. Device is rebooting...");
  delay(DeviceConfig::restartDelayMs);
  ESP.restart();
}
