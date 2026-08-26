#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_EPAPER

#include "Framebuffer.h"

// SPI pin defaults for Waveshare 7.5" HAT on MH-ET LIVE ESP32 MiniKit.
// Override in a local board wiring note if the harness differs.
namespace EPaperPins {
constexpr int busy = 4;
constexpr int rst = 16;
constexpr int dc = 17;
constexpr int cs = 5;
constexpr int clk = 18;
constexpr int din = 23;
}  // namespace EPaperPins

class Waveshare75BwDisplay {
 public:
  bool begin();
  FramebufferStatus displayFramebuffer(const uint8_t* data, size_t length);
  void sleep();
  bool isReady() const { return ready_; }

 private:
  void resetPanel();
  // Returns false if BUSY did not clear before the timeout (panel fault / wiring).
  bool waitWhileBusy(uint32_t timeoutMs = 30000);
  void sendCommand(uint8_t command);
  void sendData(uint8_t data);
  void sendDataBufferInverted(const uint8_t* data, size_t length);
  void setLut();
  bool initPanel();

  bool ready_ = false;
};

#endif  // INKADS_FEATURE_EPAPER
