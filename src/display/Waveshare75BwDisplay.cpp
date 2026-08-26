#include "Waveshare75BwDisplay.h"

#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_EPAPER

#include <SPI.h>

#include "../config/DeviceConfig.h"
#include "DisplayProfile.h"
#include "Framebuffer.h"

namespace {
constexpr uint32_t kSpiFrequencyHz = 4000000;

// Voltage / LUT tables from Waveshare epd7in5_V2 (UC8179).
const uint8_t kVoltageFrame[] = {0x06, 0x3F, 0x3F, 0x11, 0x24, 0x07, 0x17};

const uint8_t kLutVcom[] = {
    0x00, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x01, 0x0F, 0x01, 0x02,
    0x00, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t kLutWw[] = {
    0x10, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x84, 0x0F, 0x01, 0x0F, 0x01, 0x02,
    0x20, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t kLutBw[] = {
    0x10, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x84, 0x0F, 0x01, 0x0F, 0x01, 0x02,
    0x20, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t kLutWb[] = {
    0x80, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x84, 0x0F, 0x01, 0x0F, 0x01, 0x02,
    0x40, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t kLutBb[] = {
    0x80, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x84, 0x0F, 0x01, 0x0F, 0x01, 0x02,
    0x40, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
}  // namespace

bool Waveshare75BwDisplay::begin() {
  pinMode(EPaperPins::busy, INPUT);
  pinMode(EPaperPins::rst, OUTPUT);
  pinMode(EPaperPins::dc, OUTPUT);
  pinMode(EPaperPins::cs, OUTPUT);
  digitalWrite(EPaperPins::cs, HIGH);

  SPI.begin(EPaperPins::clk, -1, EPaperPins::din, EPaperPins::cs);

  resetPanel();
  if (!initPanel()) {
    ready_ = false;
    if (DeviceConfig::debugLogging) {
      Serial.println("E-paper: init timed out waiting for BUSY");
    }
    return false;
  }
  ready_ = true;
  if (DeviceConfig::debugLogging) {
    Serial.println("E-paper: Waveshare 7.5 B/W V2 ready");
    Serial.print("E-paper: profile ");
    Serial.println(DisplayProfile::kWaveshare75BwId);
  }
  return true;
}

FramebufferStatus Waveshare75BwDisplay::displayFramebuffer(const uint8_t* data, size_t length) {
  const FramebufferStatus status = validatePackedFramebuffer(data, length);
  if (status != FramebufferStatus::Ok) {
    if (DeviceConfig::debugLogging) {
      Serial.print("E-paper: reject framebuffer: ");
      Serial.println(framebufferStatusToString(status));
    }
    return status;
  }
  if (!ready_) {
    if (DeviceConfig::debugLogging) {
      Serial.println("E-paper: display called before begin()");
    }
    return FramebufferStatus::WrongSize;
  }

  // UC8179 / Waveshare 7.5" V2: new image via 0x13, refresh via 0x12.
  // Wire polarity is inverted relative to the InkAds packed buffer (bit 1 = dark).
  sendCommand(0x13);
  sendDataBufferInverted(data, length);
  sendCommand(0x12);
  delay(100);
  if (!waitWhileBusy()) {
    if (DeviceConfig::debugLogging) {
      Serial.println("E-paper: refresh BUSY timeout");
    }
    return FramebufferStatus::PanelTimeout;
  }
  if (DeviceConfig::debugLogging) {
    Serial.println("E-paper: refresh complete");
  }
  return FramebufferStatus::Ok;
}

void Waveshare75BwDisplay::sleep() {
  if (!ready_) {
    return;
  }
  sendCommand(0x02);
  waitWhileBusy();
  sendCommand(0x07);
  sendData(0xA5);
  ready_ = false;
}

void Waveshare75BwDisplay::resetPanel() {
  digitalWrite(EPaperPins::rst, HIGH);
  delay(20);
  digitalWrite(EPaperPins::rst, LOW);
  delay(4);
  digitalWrite(EPaperPins::rst, HIGH);
  delay(20);
}

bool Waveshare75BwDisplay::waitWhileBusy(uint32_t timeoutMs) {
  // Waveshare V2: BUSY pin is LOW while the controller is busy.
  const uint32_t started = millis();
  while (true) {
    sendCommand(0x71);
    if (digitalRead(EPaperPins::busy) == HIGH) {
      delay(20);
      return true;
    }
    if (millis() - started >= timeoutMs) {
      return false;
    }
    delay(10);
  }
}

void Waveshare75BwDisplay::sendCommand(uint8_t command) {
  digitalWrite(EPaperPins::dc, LOW);
  digitalWrite(EPaperPins::cs, LOW);
  SPI.beginTransaction(SPISettings(kSpiFrequencyHz, MSBFIRST, SPI_MODE0));
  SPI.transfer(command);
  SPI.endTransaction();
  digitalWrite(EPaperPins::cs, HIGH);
}

void Waveshare75BwDisplay::sendData(uint8_t data) {
  digitalWrite(EPaperPins::dc, HIGH);
  digitalWrite(EPaperPins::cs, LOW);
  SPI.beginTransaction(SPISettings(kSpiFrequencyHz, MSBFIRST, SPI_MODE0));
  SPI.transfer(data);
  SPI.endTransaction();
  digitalWrite(EPaperPins::cs, HIGH);
}

void Waveshare75BwDisplay::sendDataBufferInverted(const uint8_t* data, size_t length) {
  digitalWrite(EPaperPins::dc, HIGH);
  digitalWrite(EPaperPins::cs, LOW);
  SPI.beginTransaction(SPISettings(kSpiFrequencyHz, MSBFIRST, SPI_MODE0));
  for (size_t i = 0; i < length; ++i) {
    SPI.transfer(static_cast<uint8_t>(~data[i]));
  }
  SPI.endTransaction();
  digitalWrite(EPaperPins::cs, HIGH);
}

void Waveshare75BwDisplay::setLut() {
  sendCommand(0x20);
  for (uint8_t i = 0; i < 42; ++i) {
    sendData(kLutVcom[i]);
  }
  sendCommand(0x21);
  for (uint8_t i = 0; i < 42; ++i) {
    sendData(kLutWw[i]);
  }
  sendCommand(0x22);
  for (uint8_t i = 0; i < 42; ++i) {
    sendData(kLutBw[i]);
  }
  sendCommand(0x23);
  for (uint8_t i = 0; i < 42; ++i) {
    sendData(kLutWb[i]);
  }
  // 0x24 is LUTBB on UC8179 — not the framebuffer write path.
  sendCommand(0x24);
  for (uint8_t i = 0; i < 42; ++i) {
    sendData(kLutBb[i]);
  }
}

bool Waveshare75BwDisplay::initPanel() {
  sendCommand(0x01);  // power setting
  sendData(0x17);
  sendData(kVoltageFrame[6]);
  sendData(kVoltageFrame[1]);
  sendData(kVoltageFrame[2]);
  sendData(kVoltageFrame[3]);

  sendCommand(0x82);  // VCOM DC
  sendData(kVoltageFrame[4]);

  sendCommand(0x06);  // booster
  sendData(0x27);
  sendData(0x27);
  sendData(0x2F);
  sendData(0x17);

  sendCommand(0x30);  // OSC
  sendData(kVoltageFrame[0]);

  sendCommand(0x04);  // power on
  delay(100);
  if (!waitWhileBusy()) {
    return false;
  }

  sendCommand(0x00);  // panel setting
  sendData(0x3F);

  sendCommand(0x61);  // resolution 800×480
  sendData(0x03);
  sendData(0x20);
  sendData(0x01);
  sendData(0xE0);

  sendCommand(0x15);
  sendData(0x00);

  sendCommand(0x50);  // VCOM and data interval
  sendData(0x10);
  sendData(0x00);

  sendCommand(0x60);  // TCON
  sendData(0x22);

  sendCommand(0x65);  // resolution gate/source start
  sendData(0x00);
  sendData(0x00);
  sendData(0x00);
  sendData(0x00);

  setLut();
  return true;
}

#endif  // INKADS_FEATURE_EPAPER
