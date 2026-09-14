#ifndef CODEX_USAGE_BOOT_BUTTON_H
#define CODEX_USAGE_BOOT_BUTTON_H

#include <stdint.h>

// BOOT(GPIO0) の active-low 変換後の論理押下をデバウンスする。
enum class BootAction : uint8_t { None, Rotate, Calibrate };
// begin() に起動時の実値を渡すことで、電源投入時に押しっぱなしでも
// 偽のクリックを発生させない。
class BootButton {
 public:
  static const uint32_t kDefaultDebounceMs = 35U;

  explicit BootButton(uint32_t debounce_ms = kDefaultDebounceMs)
      : debounce_ms_(debounce_ms),
        initialized_(false),
        stable_pressed_(false),
        candidate_pressed_(false),
        candidate_since_ms_(0U), pressed_since_ms_(0U), armed_(true) {}

  void begin(bool pressed, uint32_t now) {
    initialized_ = true;
    stable_pressed_ = pressed;
    candidate_pressed_ = pressed;
    candidate_since_ms_ = now;
    pressed_since_ms_ = now;
    armed_ = !pressed;
  }

  // 離した時に短押し／1.5秒以上の長押しを一度だけ返す。
  BootAction update(bool pressed, uint32_t now) {
    if (!initialized_) {
      begin(pressed, now);
      return BootAction::None;
    }

    if (pressed != candidate_pressed_) {
      candidate_pressed_ = pressed;
      candidate_since_ms_ = now;
      return BootAction::None;
    }
    if (candidate_pressed_ == stable_pressed_ ||
        static_cast<uint32_t>(now - candidate_since_ms_) < debounce_ms_) {
      return BootAction::None;
    }

    const bool was_pressed = stable_pressed_;
    stable_pressed_ = candidate_pressed_;
    if (!was_pressed && stable_pressed_) {
      if (armed_) pressed_since_ms_ = now;
      return BootAction::None;
    }
    if (!was_pressed) return BootAction::None;
    const uint32_t duration = static_cast<uint32_t>(now - pressed_since_ms_);
    const bool valid = armed_ && duration >= 50U;
    armed_ = true;
    if (!valid) return BootAction::None;
    return duration >= 1500U ? BootAction::Calibrate : BootAction::Rotate;
  }

  bool pressed() const { return stable_pressed_; }

 private:
  uint32_t debounce_ms_;
  bool initialized_;
  bool stable_pressed_;
  bool candidate_pressed_;
  uint32_t candidate_since_ms_;
  uint32_t pressed_since_ms_;
  bool armed_;
};

#endif  // CODEX_USAGE_BOOT_BUTTON_H
