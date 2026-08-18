#pragma once

#include <Arduino.h>

#if __has_include("TlsCredentials.local.h")
#include "TlsCredentials.local.h"
#else
namespace InkAdsTlsLocal {
inline constexpr char certificatePem[] = "";
inline constexpr char privateKeyPem[] = "";
}  // namespace InkAdsTlsLocal
#endif

namespace TlsCredentials {
inline const char* certificatePem = InkAdsTlsLocal::certificatePem;
inline const char* privateKeyPem = InkAdsTlsLocal::privateKeyPem;

inline bool configured() {
  return certificatePem[0] != '\0' && privateKeyPem[0] != '\0';
}
}  // namespace TlsCredentials
