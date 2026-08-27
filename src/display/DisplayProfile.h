#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace DisplayProfile {
// Matches renderer profile id `waveshare-7.5-bw` (POC-252 / issue #2).
constexpr char kWaveshare75BwId[] = "waveshare-7.5-bw";
constexpr uint16_t kWidth = 800;
constexpr uint16_t kHeight = 480;
constexpr uint8_t kBitsPerPixel = 1;
constexpr size_t kPackedByteLength = 48000;  // 800 * 480 / 8
}  // namespace DisplayProfile
