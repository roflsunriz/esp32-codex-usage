#pragma once

#include <stdint.h>

namespace wheel_picker {

constexpr int32_t kRowHeight = 22;

inline int32_t roundedSteps(int32_t pixels) {
  if (pixels >= 0) return (pixels + kRowHeight / 2) / kRowHeight;
  return -((-pixels + kRowHeight / 2) / kRowHeight);
}

inline uint32_t clampValue(int32_t value, uint32_t maximum) {
  if (value < 0) return 0U;
  if (static_cast<uint32_t>(value) > maximum) return maximum;
  return static_cast<uint32_t>(value);
}

// Values increase down the visible drum, matching the iOS alarm picker.
inline uint32_t valueForTap(uint32_t current, int32_t touchY,
                            int32_t centerY, uint32_t maximum) {
  return clampValue(static_cast<int32_t>(current) +
                        roundedSteps(touchY - centerY),
                    maximum);
}

// Pulling the drum upward brings a larger value into the center row.
inline uint32_t valueForDrag(uint32_t start, int32_t startY,
                             int32_t currentY, uint32_t maximum) {
  return clampValue(static_cast<int32_t>(start) +
                        roundedSteps(startY - currentY),
                    maximum);
}

}  // namespace wheel_picker
