#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_ENTRA

#include "IdTokenValidator.h"

#include <cstring>
#include <cJSON.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "../config/DeviceConfig.h"
#include "../config/EntraConfig.h"
#include "CryptoUtil.h"

namespace {
constexpr size_t kMaxDecoded = 3072;
constexpr size_t kMaxSignature = 512;
constexpr size_t kMaxModulus = 512;

const char* jsonString(cJSON* object, const char* name) {
  cJSON* field = cJSON_GetObjectItemCaseSensitive(object, name);
  if (!cJSON_IsString(field) || field->valuestring == nullptr) return nullptr;
  return field->valuestring;
}

bool jsonEquals(cJSON* object, const char* name, const char* expected) {
  const char* value = jsonString(object, name);
  return value != nullptr && CryptoUtil::constantTimeEquals(value, expected);
}

cJSON* parseJson(const char* text) {
  return text == nullptr ? nullptr : cJSON_Parse(text);
}

bool timeReady() { return time(nullptr) > 1700000000; }
}  // namespace

bool IdTokenValidator::authorize(const char* idToken, const char* accessToken) {
  if (idToken == nullptr || idToken[0] == '\0') return false;
  if (!ensureJwks(false)) return false;

  bool missingRole = false;
  if (verifyJwt(idToken, EntraConfig::clientId, true, &missingRole)) {
    return true;
  }
  if (!missingRole) return false;
  if (accessToken == nullptr || accessToken[0] == '\0') return false;

  char apiAudience[96];
  snprintf(apiAudience, sizeof(apiAudience), "api://%s", EntraConfig::clientId);
  return verifyJwt(accessToken, EntraConfig::clientId, true, &missingRole) ||
         verifyJwt(accessToken, apiAudience, true, &missingRole);
}

void IdTokenValidator::clearCache() {
  free(jwksBody_);
  jwksBody_ = nullptr;
  jwksUri_[0] = '\0';
}

bool IdTokenValidator::ensureJwks(bool forceRefresh) {
  if (forceRefresh) {
    free(jwksBody_);
    jwksBody_ = nullptr;
  }
  if (jwksBody_ != nullptr) return true;

  char metadataUrl[192];
  EntraConfig::authorityUrl(metadataUrl, sizeof(metadataUrl),
                            "/v2.0/.well-known/openid-configuration");
  char* metadata = static_cast<char*>(malloc(EntraConfig::maxHttpBodyBytes));
  if (metadata == nullptr) return false;
  size_t metadataLength = 0;
  int status = 0;
  const bool metadataOk =
      https_.get(metadataUrl, metadata, EntraConfig::maxHttpBodyBytes,
                 &metadataLength, &status) &&
      status == 200;
  if (!metadataOk) {
    free(metadata);
    return false;
  }

  cJSON* document = parseJson(metadata);
  free(metadata);
  if (document == nullptr) return false;
  const char* jwksUri = jsonString(document, "jwks_uri");
  if (jwksUri == nullptr) {
    cJSON_Delete(document);
    return false;
  }
  strncpy(jwksUri_, jwksUri, sizeof(jwksUri_) - 1);
  jwksUri_[sizeof(jwksUri_) - 1] = '\0';
  cJSON_Delete(document);

  jwksBody_ = static_cast<char*>(malloc(EntraConfig::maxHttpBodyBytes));
  if (jwksBody_ == nullptr) return false;
  size_t jwksLength = 0;
  const bool jwksOk =
      https_.get(jwksUri_, jwksBody_, EntraConfig::maxHttpBodyBytes, &jwksLength,
                 &status) &&
      status == 200;
  if (!jwksOk) {
    free(jwksBody_);
    jwksBody_ = nullptr;
    return false;
  }

  if (DeviceConfig::debugLogging) {
    Serial.print("[entra] jwks refreshed free_heap=");
    Serial.println(ESP.getFreeHeap());
  }
  return true;
}

bool IdTokenValidator::verifyJwt(const char* jwt, const char* audience,
                                 bool requireRole, bool* missingRole) {
  if (verifyWithCachedKeys(jwt, audience, requireRole, missingRole)) {
    return true;
  }
  if (!ensureJwks(true)) return false;
  return verifyWithCachedKeys(jwt, audience, requireRole, missingRole);
}

bool IdTokenValidator::verifyWithCachedKeys(const char* jwt,
                                            const char* audience,
                                            bool requireRole,
                                            bool* missingRole) {
  if (missingRole != nullptr) *missingRole = false;
  if (jwt == nullptr || jwksBody_ == nullptr) return false;

  const char* firstDot = strchr(jwt, '.');
  const char* secondDot =
      firstDot == nullptr ? nullptr : strchr(firstDot + 1, '.');
  if (firstDot == nullptr || secondDot == nullptr || secondDot[1] == '\0') {
    return false;
  }
  if (strchr(secondDot + 1, '.') != nullptr) return false;

  const size_t headerLength = static_cast<size_t>(firstDot - jwt);
  const size_t payloadLength =
      static_cast<size_t>(secondDot - firstDot - 1);
  const size_t signatureLength = strlen(secondDot + 1);
  const size_t signedLength =
      static_cast<size_t>(secondDot - jwt);

  unsigned char headerJson[512];
  unsigned char payloadJson[kMaxDecoded];
  unsigned char signature[kMaxSignature];
  size_t headerJsonLength = 0;
  size_t payloadJsonLength = 0;
  size_t signatureBytes = 0;
  if (!CryptoUtil::base64UrlDecode(jwt, headerLength, headerJson,
                                   sizeof(headerJson) - 1, &headerJsonLength) ||
      !CryptoUtil::base64UrlDecode(firstDot + 1, payloadLength, payloadJson,
                                   sizeof(payloadJson) - 1,
                                   &payloadJsonLength) ||
      !CryptoUtil::base64UrlDecode(secondDot + 1, signatureLength, signature,
                                   sizeof(signature), &signatureBytes)) {
    return false;
  }
  headerJson[headerJsonLength] = '\0';
  payloadJson[payloadJsonLength] = '\0';

  cJSON* header = parseJson(reinterpret_cast<char*>(headerJson));
  if (header == nullptr) return false;
  const char* alg = jsonString(header, "alg");
  const char* kid = jsonString(header, "kid");
  if (alg == nullptr || kid == nullptr ||
      !CryptoUtil::constantTimeEquals(alg, "RS256")) {
    cJSON_Delete(header);
    return false;
  }

  cJSON* jwks = parseJson(jwksBody_);
  if (jwks == nullptr) {
    cJSON_Delete(header);
    return false;
  }
  cJSON* keys = cJSON_GetObjectItemCaseSensitive(jwks, "keys");
  cJSON* matched = nullptr;
  cJSON* key = nullptr;
  cJSON_ArrayForEach(key, keys) {
    const char* keyId = jsonString(key, "kid");
    if (keyId != nullptr && strcmp(keyId, kid) == 0) {
      matched = key;
      break;
    }
  }
  cJSON_Delete(header);
  if (matched == nullptr) {
    cJSON_Delete(jwks);
    return false;
  }

  const char* modulusB64 = jsonString(matched, "n");
  const char* exponentB64 = jsonString(matched, "e");
  if (modulusB64 == nullptr || exponentB64 == nullptr) {
    cJSON_Delete(jwks);
    return false;
  }

  unsigned char modulus[kMaxModulus];
  unsigned char exponent[16];
  size_t modulusLength = 0;
  size_t exponentLength = 0;
  const bool decodedKey =
      CryptoUtil::base64UrlDecode(modulusB64, strlen(modulusB64), modulus,
                                  sizeof(modulus), &modulusLength) &&
      CryptoUtil::base64UrlDecode(exponentB64, strlen(exponentB64), exponent,
                                  sizeof(exponent), &exponentLength);
  cJSON_Delete(jwks);
  if (!decodedKey) return false;

  if (!rsaVerify(reinterpret_cast<const unsigned char*>(jwt), signedLength,
                 signature, signatureBytes, modulus, modulusLength, exponent,
                 exponentLength)) {
    return false;
  }

  const char* payload = reinterpret_cast<char*>(payloadJson);
  if (!claimsMatch(payload, audience)) return false;
  if (!requireRole) return true;
  if (payloadHasRole(payload)) return true;
  if (missingRole != nullptr) *missingRole = true;
  return false;
}

bool IdTokenValidator::rsaVerify(const unsigned char* signedData,
                                 size_t signedLength,
                                 const unsigned char* signature,
                                 size_t signatureLength,
                                 const unsigned char* modulus,
                                 size_t modulusLength,
                                 const unsigned char* exponent,
                                 size_t exponentLength) {
  unsigned char hash[32];
  if (mbedtls_sha256(signedData, signedLength, hash, 0) != 0) return false;

  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa);
  mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);
  const bool imported =
      mbedtls_rsa_import_raw(&rsa, modulus, modulusLength, nullptr, 0, nullptr,
                             0, nullptr, 0, exponent, exponentLength) == 0 &&
      mbedtls_rsa_complete(&rsa) == 0 && mbedtls_rsa_check_pubkey(&rsa) == 0;
  bool verified = false;
  if (imported && signatureLength == mbedtls_rsa_get_len(&rsa)) {
    verified = mbedtls_rsa_pkcs1_verify(&rsa, MBEDTLS_MD_SHA256, sizeof(hash),
                                        hash, signature) == 0;
  }
  mbedtls_rsa_free(&rsa);
  return verified;
}

bool IdTokenValidator::payloadHasRole(const char* payloadJson) {
  cJSON* payload = parseJson(payloadJson);
  if (payload == nullptr) return false;
  cJSON* roles = cJSON_GetObjectItemCaseSensitive(payload, "roles");
  bool allowed = false;
  if (cJSON_IsString(roles) && roles->valuestring != nullptr) {
    allowed = CryptoUtil::constantTimeEquals(roles->valuestring,
                                             EntraConfig::administratorRole);
  } else if (cJSON_IsArray(roles)) {
    cJSON* role = nullptr;
    cJSON_ArrayForEach(role, roles) {
      if (cJSON_IsString(role) && role->valuestring != nullptr &&
          CryptoUtil::constantTimeEquals(role->valuestring,
                                         EntraConfig::administratorRole)) {
        allowed = true;
        break;
      }
    }
  }
  cJSON_Delete(payload);
  return allowed;
}

bool IdTokenValidator::audienceMatches(const char* payloadJson,
                                       const char* audience) {
  cJSON* payload = parseJson(payloadJson);
  if (payload == nullptr) return false;
  cJSON* aud = cJSON_GetObjectItemCaseSensitive(payload, "aud");
  bool matches = false;
  if (cJSON_IsString(aud) && aud->valuestring != nullptr) {
    matches = CryptoUtil::constantTimeEquals(aud->valuestring, audience);
  } else if (cJSON_IsArray(aud)) {
    cJSON* value = nullptr;
    cJSON_ArrayForEach(value, aud) {
      if (cJSON_IsString(value) && value->valuestring != nullptr &&
          CryptoUtil::constantTimeEquals(value->valuestring, audience)) {
        matches = true;
        break;
      }
    }
  }
  cJSON_Delete(payload);
  return matches;
}

bool IdTokenValidator::claimsMatch(const char* payloadJson,
                                   const char* audience) {
  if (!timeReady()) return false;
  cJSON* payload = parseJson(payloadJson);
  if (payload == nullptr) return false;

  char issuer[128];
  EntraConfig::expectedIssuer(issuer, sizeof(issuer));
  const bool issuerOk = jsonEquals(payload, "iss", issuer);
  const bool tenantOk = jsonEquals(payload, "tid", EntraConfig::tenantId);
  cJSON* expField = cJSON_GetObjectItemCaseSensitive(payload, "exp");
  cJSON* nbfField = cJSON_GetObjectItemCaseSensitive(payload, "nbf");
  const time_t now = time(nullptr);
  bool expiryOk = cJSON_IsNumber(expField) &&
                  now <= static_cast<time_t>(expField->valuedouble) +
                             EntraConfig::clockSkewSeconds;
  bool notBeforeOk =
      nbfField == nullptr ||
      (cJSON_IsNumber(nbfField) &&
       now + EntraConfig::clockSkewSeconds >=
           static_cast<time_t>(nbfField->valuedouble));
  cJSON_Delete(payload);
  return issuerOk && tenantOk && expiryOk && notBeforeOk &&
         audienceMatches(payloadJson, audience);
}

#endif
