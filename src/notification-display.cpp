#include "notification-display.h"

#include <Preferences.h>
#include <algorithm>
#include <cstdlib>

namespace {
constexpr int kTouchSck = 25, kTouchMiso = 39, kTouchMosi = 32;
constexpr int kTouchCs = 33, kTouchIrq = 36;
constexpr int16_t kCapturePressure = 12;
constexpr uint32_t kCalibrationVersion = 1;
struct StoredCalibration {
  uint32_t version;
  int16_t values[5];
  uint16_t check;
};
static_assert(sizeof(StoredCalibration) == 16, "touch calibration record size");
uint16_t calibrationCheck(const StoredCalibration& stored) {
  uint16_t result = 0xA53C;
  for (const auto value : stored.values) result ^= static_cast<uint16_t>(value);
  return result;
}
}

CodexUsageDisplay::CodexUsageDisplay()
    : touchBus_(VSPI), touch_(kTouchCs, kTouchIrq, 120) {}

int16_t CodexUsageDisplay::mapAxis(int16_t raw, int16_t start, int16_t end,
                                    int16_t targetStart, int16_t targetEnd,
                                    int16_t maximum) {
  const long span = static_cast<long>(end) - start;
  if (std::abs(span) < 100) return targetStart;
  long mapped = targetStart + (static_cast<long>(raw) - start) *
                                (targetEnd - targetStart) / span;
  return static_cast<int16_t>(std::max<long>(0, std::min<long>(maximum, mapped)));
}

void CodexUsageDisplay::loadCalibration() {
  Preferences prefs;
  if (!prefs.begin("usage-touch", true)) return;
  StoredCalibration stored = {};
  if (prefs.getBytesLength("calib") == sizeof(stored) &&
      prefs.getBytes("calib", &stored, sizeof(stored)) == sizeof(stored) &&
      stored.version == kCalibrationVersion &&
      stored.check == calibrationCheck(stored)) {
    Calibration candidate{
        stored.values[0], stored.values[1], stored.values[2],
        stored.values[3], stored.values[4]};
    if (std::abs(candidate.right - candidate.left) > 1000 &&
        std::abs(candidate.bottom - candidate.top) > 1000 &&
        candidate.pressure >= kCapturePressure && candidate.pressure <= 120)
      calibration_ = candidate;
  }
  prefs.end();
}

bool CodexUsageDisplay::saveCalibration() {
  Preferences prefs;
  if (!prefs.begin("usage-touch", false)) return false;
  StoredCalibration stored{kCalibrationVersion,
      {calibration_.left, calibration_.right, calibration_.top,
       calibration_.bottom, calibration_.pressure}, 0};
  stored.check = calibrationCheck(stored);
  const bool saved = prefs.putBytes("calib", &stored, sizeof(stored)) == sizeof(stored);
  prefs.end();
  return saved;
}

void CodexUsageDisplay::beginTouch() {
  pwmReady_ = ledcAttach(TFT_BL, 5000, 8);
  if (!pwmReady_) pinMode(TFT_BL, OUTPUT);
  setBrightness(255);
  touchBus_.begin(kTouchSck, kTouchMiso, kTouchMosi, kTouchCs);
  touch_.begin(touchBus_);
  touch_.setRotation(1);
  loadCalibration();
  touch_.setPressureThreshold(calibration_.pressure);
}

void CodexUsageDisplay::setBrightness(uint8_t value) {
  brightness_ = value;
  if (pwmReady_) ledcWrite(TFT_BL, value);
  else digitalWrite(TFT_BL, value ? HIGH : LOW);
}

bool CodexUsageDisplay::getTouch(uint16_t* x, uint16_t* y) {
  if (!touch_.tirqTouched()) {
    sampleCount_ = 0;
    contactDelivered_ = false;
    sumX_ = sumY_ = 0;
    return false;
  }
  const SensitiveTouchPoint point = touch_.getPoint();
  if (contactDelivered_) {
    if (x) *x = capturedX_;
    if (y) *y = capturedY_;
    return true;
  }
  if (point.z < calibration_.pressure) return false;
  const int16_t mappedX = mapAxis(point.x, calibration_.left, calibration_.right,
                                   24, 295, 319);
  const int16_t mappedY = mapAxis(point.y, calibration_.top, calibration_.bottom,
                                   24, 215, 239);
  if (sampleCount_ != 0 &&
      (std::abs(mappedX - sumX_ / sampleCount_) > 18 ||
       std::abs(mappedY - sumY_ / sampleCount_) > 18)) {
    sampleCount_ = 0; sumX_ = sumY_ = 0;
  }
  sumX_ += mappedX; sumY_ += mappedY;
  if (++sampleCount_ < 3) return false;
  int16_t stableX = static_cast<int16_t>(sumX_ / sampleCount_);
  int16_t stableY = static_cast<int16_t>(sumY_ / sampleCount_);
  if (getRotation() == 3) { stableX = 319 - stableX; stableY = 239 - stableY; }
  capturedX_ = stableX; capturedY_ = stableY;
  contactDelivered_ = true;
  if (x) *x = capturedX_;
  if (y) *y = capturedY_;
  return true;
}

bool CodexUsageDisplay::capturePoint(int16_t& x, int16_t& y, int16_t& pressure) {
  const uint32_t start = millis();
  while (static_cast<uint32_t>(millis() - start) < 15000) {
    if (!touch_.tirqTouched()) { delay(10); continue; }
    int32_t sx = 0, sy = 0;
    int16_t count = 0, weakest = 32767;
    while (touch_.tirqTouched() && count < 12 &&
           static_cast<uint32_t>(millis() - start) < 15000) {
      const SensitiveTouchPoint p = touch_.getPoint();
      if (p.z >= kCapturePressure) {
        sx += p.x; sy += p.y;
        weakest = std::min(weakest, p.z);
        ++count;
      }
      delay(12);
    }
    while (touch_.tirqTouched() &&
           static_cast<uint32_t>(millis() - start) < 15000) {
      touch_.getPoint(); delay(10);
    }
    if (count >= 4) {
      x = static_cast<int16_t>(sx / count);
      y = static_cast<int16_t>(sy / count);
      pressure = weakest;
      return true;
    }
  }
  return false;
}

void CodexUsageDisplay::calibrateTouch() {
  setBrightness(255);
  const uint8_t savedRotation = getRotation();
  setRotation(1);
  touch_.setPressureThreshold(kCapturePressure);
  auto step = [this](const char* title, int x, int y) {
    fillScreen(TFT_BLACK);
    setTextColor(TFT_WHITE, TFT_BLACK);
    drawString(title, 8, 8, 2);
    drawString("Press cross with stylus", 8, 40, 2);
    fillRect(x - 10, y, 21, 1, TFT_YELLOW);
    fillRect(x, y - 10, 1, 21, TFT_YELLOW);
  };
  int16_t left = 0, top = 0, right = 0, bottom = 0, z1 = 0, z2 = 0;
  step("TOUCH 1/2", 24, 24);
  const bool first = capturePoint(left, top, z1);
  if (first) step("TOUCH 2/2", 295, 215);
  const bool second = first && capturePoint(right, bottom, z2);
  if (second && std::abs(right - left) > 1000 &&
      std::abs(bottom - top) > 1000) {
    const int16_t weakest = std::min(z1, z2);
    const Calibration previous = calibration_;
    calibration_ = {left, right, top, bottom,
                    static_cast<int16_t>(std::max<int>(12, std::min<int>(120, weakest / 2)))};
    if (!saveCalibration()) {
      calibration_ = previous;
      fillScreen(TFT_BLACK);
      drawString("Calibration save failed", 8, 70, 2);
      delay(1800);
    }
  } else {
    fillScreen(TFT_BLACK);
    setTextColor(TFT_YELLOW, TFT_BLACK);
    drawString(first ? "Calibration invalid" : "No touch detected", 8, 70, 2);
    delay(1800);
  }
  touch_.setPressureThreshold(calibration_.pressure);
  setRotation(savedRotation);
  contactDelivered_ = false;
  sampleCount_ = 0;
}
