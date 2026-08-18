#pragma once

#include <Arduino.h>
#include <cstring>
#include <esp_random.h>
#include <mbedtls/base64.h>

namespace CryptoUtil {
inline bool constantTimeEquals(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return false;
  const size_t leftLength = strlen(left);
  const size_t rightLength = strlen(right);
  if (leftLength != rightLength) return false;
  uint8_t difference = 0;
  for (size_t index = 0; index < leftLength; ++index) {
    difference |= static_cast<uint8_t>(left[index] ^ right[index]);
  }
  return difference == 0;
}

inline void toHex(const uint8_t* bytes, size_t length, char* out,
                  size_t outSize) {
  if (outSize < length * 2 + 1) {
    if (outSize > 0) out[0] = '\0';
    return;
  }
  static const char digits[] = "0123456789abcdef";
  for (size_t index = 0; index < length; ++index) {
    out[index * 2] = digits[bytes[index] >> 4];
    out[index * 2 + 1] = digits[bytes[index] & 0x0f];
  }
  out[length * 2] = '\0';
}

inline void randomHex(char* out, size_t byteCount) {
  uint8_t bytes[32];
  if (byteCount > sizeof(bytes)) byteCount = sizeof(bytes);
  esp_fill_random(bytes, byteCount);
  toHex(bytes, byteCount, out, byteCount * 2 + 1);
}

inline bool base64UrlDecode(const char* input, size_t inputLength,
                            unsigned char* output, size_t outputSize,
                            size_t* outputLength) {
  if (input == nullptr || output == nullptr || outputLength == nullptr) {
    return false;
  }

  const size_t padding = (4 - (inputLength % 4)) % 4;
  String normalized;
  normalized.reserve(inputLength + padding);
  for (size_t index = 0; index < inputLength; ++index) {
    const char character = input[index];
    if (character == '-') {
      normalized += '+';
    } else if (character == '_') {
      normalized += '/';
    } else {
      normalized += character;
    }
  }
  for (size_t index = 0; index < padding; ++index) normalized += '=';

  return mbedtls_base64_decode(output, outputSize, outputLength,
                               reinterpret_cast<const unsigned char*>(
                                   normalized.c_str()),
                               normalized.length()) == 0;
}

inline void secureWipe(char* buffer, size_t length) {
  if (buffer == nullptr) return;
  memset(buffer, 0, length);
}
}  // namespace CryptoUtil
