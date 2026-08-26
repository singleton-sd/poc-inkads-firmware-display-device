#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "DisplayProfile.h"

enum class FramebufferStatus : uint8_t {
  Ok = 0,
  NullPointer,
  WrongSize,
};

inline FramebufferStatus validatePackedFramebuffer(const uint8_t* data, size_t length) {
  if (data == nullptr) {
    return FramebufferStatus::NullPointer;
  }
  if (length != DisplayProfile::kPackedByteLength) {
    return FramebufferStatus::WrongSize;
  }
  return FramebufferStatus::Ok;
}

inline const char* framebufferStatusToString(FramebufferStatus status) {
  switch (status) {
    case FramebufferStatus::Ok:
      return "ok";
    case FramebufferStatus::NullPointer:
      return "null_pointer";
    case FramebufferStatus::WrongSize:
      return "wrong_size";
    default:
      return "unknown";
  }
}
