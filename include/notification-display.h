#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <sensitive-xpt2046.h>

// Codex Micro と同じ TFT_eSPI + 別 VSPI の XPT2046 入力。
class CodexUsageDisplay : public TFT_eSPI {
 public:
  CodexUsageDisplay();
  void beginTouch();
  bool getTouch(uint16_t* x, uint16_t* y);
  void calibrateTouch();
  void setBrightness(uint8_t value);
  uint8_t getBrightness() const { return brightness_; }

 private:
  struct Calibration {
    int16_t left = 200, right = 3700, top = 240, bottom = 3800;
    int16_t pressure = 120;
  } calibration_;
  void loadCalibration();
  bool saveCalibration();
  bool capturePoint(int16_t& x, int16_t& y, int16_t& pressure);
  static int16_t mapAxis(int16_t raw, int16_t start, int16_t end,
                         int16_t targetStart, int16_t targetEnd, int16_t maximum);
  SPIClass touchBus_;
  SensitiveXpt2046 touch_;
  uint8_t brightness_ = 255;
  bool pwmReady_ = false;
  bool contactDelivered_ = false;
  int32_t sumX_ = 0, sumY_ = 0;
  uint8_t sampleCount_ = 0;
  uint16_t capturedX_ = 0, capturedY_ = 0;
};
