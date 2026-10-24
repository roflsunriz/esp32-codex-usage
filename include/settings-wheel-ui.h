#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "wheel-picker.h"

namespace settings_ui {

constexpr uint16_t kSliderX0 = 24U;
constexpr uint16_t kSliderX1 = 275U;
constexpr uint16_t kPickerLeftX0 = 28U;
constexpr uint16_t kPickerLeftX1 = 146U;
constexpr uint16_t kPickerRightX0 = 154U;
constexpr uint16_t kPickerRightX1 = 272U;
constexpr uint16_t kPickerCenterY = 142U;
constexpr uint16_t kPickerTopY =
    kPickerCenterY - wheel_picker::kRowHeight * 2U - 11U;
constexpr uint16_t kPickerBottomY =
    kPickerCenterY + wheel_picker::kRowHeight * 2U + 11U;
constexpr uint16_t kSliderPollY = 232U;
constexpr int32_t kContentH = 382;
constexpr int32_t kVisibleTop = 38;
constexpr int32_t kVisibleBottom = 208;
constexpr int32_t kScrollMax = kContentH - (kVisibleBottom - kVisibleTop);
constexpr uint16_t kScrollBarX0 = 283U;
constexpr uint16_t kScrollBarY0 = 44U;
constexpr uint16_t kScrollBarY1 = 204U;
constexpr int32_t kScrollPage = 40;
constexpr uint16_t kUsageWakeActionY = 264U;
constexpr uint16_t kScreenOffActionY = 302U;
constexpr uint16_t kSetupActionY = 340U;
constexpr uint16_t kActionW = 268U;

inline uint32_t sliderValueFromX(uint16_t x, uint32_t minV, uint32_t maxV,
                                 uint32_t step) {
  if (maxV <= minV || step == 0U) return minV;
  const uint32_t trackW = kSliderX1 - kSliderX0;
  uint32_t offset = x < kSliderX0 ? 0U : x - kSliderX0;
  if (offset > trackW) offset = trackW;
  const uint32_t steps = (maxV - minV) / step;
  uint32_t index = (offset * steps + trackW / 2U) / trackW;
  if (index > steps) index = steps;
  return minV + index * step;
}

inline uint16_t sliderXFromValue(uint32_t value, uint32_t minV,
                                 uint32_t maxV) {
  if (maxV <= minV) return kSliderX0;
  if (value < minV) value = minV;
  if (value > maxV) value = maxV;
  return static_cast<uint16_t>(
      kSliderX0 + (value - minV) * (kSliderX1 - kSliderX0) / (maxV - minV));
}

inline int32_t clampScroll(int32_t scroll) {
  if (scroll < 0) return 0;
  if (scroll > kScrollMax) return kScrollMax;
  return scroll;
}

template <typename Surface, typename ContentY>
void drawWheel(Surface& surface, ContentY contentY, uint16_t x0, uint16_t x1,
               uint32_t value, uint32_t maximum, const String& label,
               uint16_t text, uint16_t muted, uint16_t background,
               uint16_t selectedBackground) {
  const int16_t labelY = contentY(74);
  if (labelY >= 56 && labelY <= 192) {
    const int32_t width = surface.textWidth(label);
    surface.setTextColor(muted, background);
    surface.drawString(label, static_cast<uint16_t>(x0 + ((x1 - x0) - width) / 2),
                       static_cast<uint16_t>(labelY));
  }
  const int16_t center = contentY(kPickerCenterY);
  if (center >= 68 && center <= 182) {
    surface.fillRoundRect(
        x0, static_cast<uint16_t>(center - wheel_picker::kRowHeight / 2),
        x1 - x0, wheel_picker::kRowHeight, 3, selectedBackground);
  }
  for (int32_t offset = -2; offset <= 2; ++offset) {
    const int32_t candidate = static_cast<int32_t>(value) + offset;
    if (candidate < 0 || candidate > static_cast<int32_t>(maximum)) continue;
    const int16_t y = static_cast<int16_t>(
        center + offset * wheel_picker::kRowHeight - 8);
    if (y < 56 || y > 192) continue;
    String shown = candidate < 10 ? String("0") : String();
    shown += String(candidate);
    const int32_t width = surface.textWidth(shown);
    surface.setTextColor(offset == 0 ? text : muted,
                         offset == 0 ? selectedBackground : background);
    surface.drawString(shown,
                       static_cast<uint16_t>(x0 + ((x1 - x0) - width) / 2),
                       static_cast<uint16_t>(y));
  }
}

}  // namespace settings_ui
