#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_EPAPER

#include "DisplayProfile.h"

// Fills a 48KB packed 1bpp buffer with a deterministic checkerboard for panel
// bring-up (no cloud download required).
inline void fillCheckerboardFixture(uint8_t* out, size_t length) {
  if (out == nullptr || length != DisplayProfile::kPackedByteLength) {
    return;
  }

  constexpr uint16_t block = 32;
  for (uint16_t y = 0; y < DisplayProfile::kHeight; ++y) {
    for (uint16_t xByte = 0; xByte < DisplayProfile::kWidth / 8; ++xByte) {
      uint8_t byteValue = 0;
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint16_t x = static_cast<uint16_t>(xByte * 8 + bit);
        const bool dark = ((x / block) + (y / block)) % 2 == 0;
        if (dark) {
          byteValue |= static_cast<uint8_t>(0x80 >> bit);
        }
      }
      out[y * (DisplayProfile::kWidth / 8) + xByte] = byteValue;
    }
  }
}

#endif  // INKADS_FEATURE_EPAPER
