#ifndef CODEX_USAGE_BOOT_BUTTON_H
#define CODEX_USAGE_BOOT_BUTTON_H

#include <stdint.h>

// BOOT(GPIO0) の active-low 変換後の論理押下をデバウンスする。
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
        candidate_since_ms_(0U) {}

  void begin(bool pressed, uint32_t now) {
    initialized_ = true;
    stable_pressed_ = pressed;
    candidate_pressed_ = pressed;
    candidate_since_ms_ = now;
  }

  // 安定した押下の立ち上がりだけを返す。
  bool update(bool pressed, uint32_t now) {
    if (!initialized_) {
      begin(pressed, now);
      return false;
    }

    if (pressed != candidate_pressed_) {
      candidate_pressed_ = pressed;
      candidate_since_ms_ = now;
      return false;
    }
    if (candidate_pressed_ == stable_pressed_ ||
        static_cast<uint32_t>(now - candidate_since_ms_) < debounce_ms_) {
      return false;
    }

    const bool was_pressed = stable_pressed_;
    stable_pressed_ = candidate_pressed_;
    return !was_pressed && stable_pressed_;
  }

  bool pressed() const { return stable_pressed_; }

 private:
  uint32_t debounce_ms_;
  bool initialized_;
  bool stable_pressed_;
  bool candidate_pressed_;
  uint32_t candidate_since_ms_;
};

#endif  // CODEX_USAGE_BOOT_BUTTON_H
