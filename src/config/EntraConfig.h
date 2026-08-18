#pragma once

// These identifiers are public OAuth client configuration, not secrets.
// Populate them after creating the single-tenant Entra app registration.
namespace EntraConfig {
inline constexpr char tenantId[] = "";
inline constexpr char clientId[] = "";
inline constexpr char administratorRole[] = "InkAds.Admin";

inline bool configured() {
  return tenantId[0] != '\0' && clientId[0] != '\0';
}
}  // namespace EntraConfig
