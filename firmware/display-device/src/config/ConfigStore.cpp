#include "ConfigStore.h"

#include <Preferences.h>

#include "DeviceConfig.h"
#include "../platform/DebugLog.h"

DeviceSettings ConfigStore::load() const {
  Preferences preferences;
  DeviceSettings credentials;

  if (!preferences.begin(DeviceConfig::preferencesNamespace, true)) {
    return credentials;
  }

  credentials.ssid = preferences.getString(DeviceConfig::ssidKey, "");
  credentials.password = preferences.getString(DeviceConfig::passwordKey, "");
  credentials.adminPassword =
      preferences.getString(DeviceConfig::adminPasswordKey, "");
  preferences.end();
  DebugLog::credentials("loaded from persistent memory", credentials);
  return credentials;
}

bool ConfigStore::save(const DeviceSettings& credentials) const {
  if (!credentials.isConfigured() ||
      credentials.ssid.length() > DeviceConfig::maxSsidLength ||
      credentials.password.length() > DeviceConfig::maxPasswordLength ||
      credentials.adminPassword.length() <
          DeviceConfig::minAdminPasswordLength ||
      credentials.adminPassword.length() >
          DeviceConfig::maxAdminPasswordLength) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(DeviceConfig::preferencesNamespace, false)) {
    return false;
  }

  const size_t ssidBytes =
      preferences.putString(DeviceConfig::ssidKey, credentials.ssid);
  const size_t passwordBytes =
      preferences.putString(DeviceConfig::passwordKey, credentials.password);
  const size_t adminPasswordBytes = preferences.putString(
      DeviceConfig::adminPasswordKey, credentials.adminPassword);
  preferences.end();

  const bool passwordSaved = credentials.password.isEmpty() || passwordBytes > 0;
  const bool saved = ssidBytes > 0 && passwordSaved && adminPasswordBytes > 0;
  if (DeviceConfig::debugLogging) {
    Serial.print("[config] persistent save result=");
    Serial.println(saved ? "success" : "failure");
  }
  return saved;
}

bool ConfigStore::clear() const {
  Preferences preferences;
  if (!preferences.begin(DeviceConfig::preferencesNamespace, false)) {
    return false;
  }

  const bool cleared = preferences.clear();
  preferences.end();
  return cleared;
}
