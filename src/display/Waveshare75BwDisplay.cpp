#include "Waveshare75BwDisplay.h"

#include "../config/InkAdsFeatures.h"

#if INKADS_FEATURE_EPAPER

#include <SPI.h>

#include "../config/DeviceConfig.h"
#include "DisplayProfile.h"
#include "Framebuffer.h"

namespace {
constexpr uint32_t kSpiFrequencyHz = 4000000;
}

bool Waveshare75BwDisplay::begin() {
  pinMode(EPaperPins::busy, INPUT);
  pinMode(EPaperPins::rst, OUTPUT);
  pinMode(EPaperPins::dc, OUTPUT);
  pinMode(EPaperPins::cs, OUTPUT);
  digitalWrite(EPaperPins::cs, HIGH);

  SPI.begin(EPaperPins::clk, -1, EPaperPins::din, EPaperPins::cs);

  resetPanel();
  initPanel();
  ready_ = true;
  if (DeviceConfig::debugLogging) {
    Serial.println("E-paper: Waveshare 7.5 B/W ready");
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

  // UC8179 / Waveshare 7.5" V2: write black RAM then refresh.
  sendCommand(0x24);
  sendDataBuffer(data, length);
  sendCommand(0x22);
  sendData(0xF7);
  sendCommand(0x20);
  waitWhileBusy();
  if (DeviceConfig::debugLogging) {
    Serial.println("E-paper: refresh complete");
  }
  return FramebufferStatus::Ok;
}

void Waveshare75BwDisplay::sleep() {
  if (!ready_) {
    return;
  }
  sendCommand(0x10);
  sendData(0x01);
  delay(100);
}

void Waveshare75BwDisplay::resetPanel() {
  digitalWrite(EPaperPins::rst, HIGH);
  delay(20);
  digitalWrite(EPaperPins::rst, LOW);
  delay(2);
  digitalWrite(EPaperPins::rst, HIGH);
  delay(20);
}

void Waveshare75BwDisplay::waitWhileBusy() {
  while (digitalRead(EPaperPins::busy) == HIGH) {
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

void Waveshare75BwDisplay::sendDataBuffer(const uint8_t* data, size_t length) {
  digitalWrite(EPaperPins::dc, HIGH);
  digitalWrite(EPaperPins::cs, LOW);
  SPI.beginTransaction(SPISettings(kSpiFrequencyHz, MSBFIRST, SPI_MODE0));
  for (size_t i = 0; i < length; ++i) {
    SPI.transfer(data[i]);
  }
  SPI.endTransaction();
  digitalWrite(EPaperPins::cs, HIGH);
}

void Waveshare75BwDisplay::initPanel() {
  waitWhileBusy();
  sendCommand(0x12);  // soft reset
  waitWhileBusy();

  sendCommand(0x01);  // driver output control
  sendData(0xDF);
  sendData(0x01);
  sendData(0x00);

  sendCommand(0x11);  // data entry mode
  sendData(0x03);

  sendCommand(0x44);  // set RAM X: 0 .. (800/8 - 1)
  sendData(0x00);
  sendData(0x63);

  sendCommand(0x45);  // set RAM Y: 0 .. 479
  sendData(0x00);
  sendData(0x00);
  sendData(0xDF);
  sendData(0x01);

  sendCommand(0x3C);  // border waveform
  sendData(0x05);

  sendCommand(0x18);
  sendData(0x80);

  sendCommand(0x4E);
  sendData(0x00);
  sendCommand(0x4F);
  sendData(0x00);
  sendData(0x00);
  waitWhileBusy();
}

#endif  // INKADS_FEATURE_EPAPER
