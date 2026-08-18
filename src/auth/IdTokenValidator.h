#pragma once

#include "EntraHttpsClient.h"

class IdTokenValidator {
 public:
  bool authorize(const char* idToken, const char* accessToken);
  void clearCache();

 private:
  bool ensureJwks(bool forceRefresh);
  bool verifyJwt(const char* jwt, const char* audience, bool requireRole,
                 bool* missingRole);
  bool verifyWithCachedKeys(const char* jwt, const char* audience,
                            bool requireRole, bool* missingRole);
  bool rsaVerify(const unsigned char* signedData, size_t signedLength,
                 const unsigned char* signature, size_t signatureLength,
                 const unsigned char* modulus, size_t modulusLength,
                 const unsigned char* exponent, size_t exponentLength);
  bool payloadHasRole(const char* payloadJson);
  bool audienceMatches(const char* payloadJson, const char* audience);
  bool claimsMatch(const char* payloadJson, const char* audience);

  EntraHttpsClient https_;
  char jwksUri_[192] = {};
  char* jwksBody_ = nullptr;
};
