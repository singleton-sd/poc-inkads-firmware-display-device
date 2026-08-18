#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "../config/ConfigStore.h"

class ProvisioningPortal {
 public:
  explicit ProvisioningPortal(ConfigStore& configStore);

  bool begin();
  void loop();
  const String& accessPointName() const;

 private:
  void showPortal();
  void saveCredentials();
  void redirectToPortal();
  String buildAccessPointName() const;

  ConfigStore& configStore_;
  DNSServer dnsServer_;
  WebServer webServer_{80};
  String accessPointName_;
};
