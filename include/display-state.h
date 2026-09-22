#ifndef CODEX_USAGE_DISPLAY_STATE_H
#define CODEX_USAGE_DISPLAY_STATE_H

#include <stdint.h>

// UI の表示状態だけを扱う、表示ドライバ非依存の状態機械。0 の指定は
// 自動消灯オフ（常時点灯）と同じで、期限切れにならない。
class DisplayState {
 public:
  // 新しい自動消灯メニューのスライダー範囲：0〜59分＋0〜24時間。
  // 合計は 時*3600＋分*60（ミリ秒換算）で、0分0時間は常時点灯になる。
  // 取得間隔スライダーは60〜600秒の60秒刻み。
  static constexpr uint32_t kSleepMinutesMax = 59U;
  static constexpr uint32_t kSleepHoursMax = 24U;
  static constexpr uint32_t kSleepTimeoutMaxMs =
      (24U * 3600U + 59U * 60U) * 1000U;
  static constexpr uint32_t kPollSliderMinSec = 60U;
  static constexpr uint32_t kPollSliderMaxSec = 600U;
  static constexpr uint32_t kPollSliderStepSec = 60U;

  static uint32_t sleepMinutesPart(uint32_t timeoutMs) {
    return (timeoutMs % 3600000U) / 60000U;
  }

  static uint32_t sleepHoursPart(uint32_t timeoutMs) {
    return timeoutMs / 3600000U;
  }

  static uint32_t sleepTimeoutFromParts(uint32_t minutes, uint32_t hours) {
    if (minutes > kSleepMinutesMax) minutes = kSleepMinutesMax;
    if (hours > kSleepHoursMax) hours = kSleepHoursMax;
    return (hours * 3600U + minutes * 60U) * 1000U;
  }

  static bool isValidPollSliderSec(uint32_t pollSec) {
    return pollSec >= kPollSliderMinSec && pollSec <= kPollSliderMaxSec &&
           pollSec % kPollSliderStepSec == 0U;
  }
  // 起動時は表示中とし、既定の自動消灯時間を 1 分にする。
  explicit DisplayState(uint32_t now = 0U)
      : timeout_ms_(60000U),
        last_activity_ms_(now),
        awake_(true),
        pressed_(false),
        wake_gesture_(false) {}

  // now は millis() などの uint32_t タイマー値を渡す。差分計算は
  // unsigned のラップアラウンドを利用するため、タイマー巻き戻り後も動作する。
  void tick(uint32_t now) {
    if (!awake_ || timeout_ms_ == 0U ||
        static_cast<uint32_t>(now - last_activity_ms_) < timeout_ms_) {
      return;
    }

    awake_ = false;
    // 消灯境界でタッチを保持していた場合も、そのジェスチャーを起床専用にする。
    wake_gesture_ = pressed_;
  }

  // pressed の立ち上がりだけを返す。消灯中の最初のジェスチャーは起床専用で、
  // release されるまで、また次の立ち上がりまで操作として扱わない。
  bool touch(bool pressed, uint32_t now) {
    // 呼び出し側が別途 tick していなくても、タッチ時点の期限を正しく反映する。
    tick(now);

    const bool new_press = pressed && !pressed_;
    bool actionable = false;

    if (!awake_) {
      if (new_press) {
        awake_ = true;
        last_activity_ms_ = now;
        wake_gesture_ = true;
      } else if (!pressed) {
        // 起床に使ったジェスチャーの終了を確認してから操作を許可する。
        wake_gesture_ = false;
      }
    } else if (wake_gesture_) {
      if (!pressed) {
        wake_gesture_ = false;
      }
    } else if (new_press) {
      last_activity_ms_ = now;
      actionable = true;
    }

    pressed_ = pressed;
    return actionable;
  }

  // 外部の物理ボタンなど、タッチ以外の操作で表示を起こす。
  void wake(uint32_t now) {
    awake_ = true;
    last_activity_ms_ = now;
    // 押しっぱなしのタッチが起床直後の操作にならないようにする。
    wake_gesture_ = pressed_;
  }

  // 許可された選択肢だけを受け付け、成功時は変更時刻からカウントし直す。
  bool setTimeout(uint32_t timeout_ms, uint32_t now) {
    if (!isValidTimeout(timeout_ms)) {
      return false;
    }

    timeout_ms_ = timeout_ms;
    last_activity_ms_ = now;
    return true;
  }

  uint32_t timeout() const { return timeout_ms_; }

  bool awake() const { return awake_; }

 private:
  // 0（常時点灯）から89940000（24時間59分）までの整数を受け付ける。
  // 旧9択の値はすべて範囲内のため、保存済み設定はそのまま有効である。
  static bool isValidTimeout(uint32_t timeout_ms) {
    return timeout_ms <= kSleepTimeoutMaxMs;
  }

  uint32_t timeout_ms_;
  uint32_t last_activity_ms_;
  bool awake_;
  bool pressed_;
  bool wake_gesture_;
};

#endif  // CODEX_USAGE_DISPLAY_STATE_H
