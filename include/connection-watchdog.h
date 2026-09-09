#ifndef CODEX_USAGE_CONNECTION_WATCHDOG_H
#define CODEX_USAGE_CONNECTION_WATCHDOG_H

#include <stdint.h>

namespace usage {

// Wi-Fiの関連付け状態とIP取得状態を分離して監視する。
// tick() がtrueを返した回だけ、呼び出し側が再接続を要求する。
class ConnectionWatchdog {
 public:
  bool tick(bool associated, bool hasIp, uint32_t now) {
    if (!initialized_) {
      initialized_ = true;
      last_associated_ = associated;
      last_has_ip_ = hasIp;
      since_ = now;
      return false;
    }

    if (associated != last_associated_ || hasIp != last_has_ip_) {
      since_ = now;
      last_associated_ = associated;
      last_has_ip_ = hasIp;
    }

    if (hasIp) {
      return false;
    }

    const uint32_t grace = associated ? 120000U : 30000U;
    if (static_cast<uint32_t>(now - since_) < grace) {
      return false;
    }

    // 通知後も同じ状態が続く場合は、次の猶予期間まで再通知しない。
    since_ = now;
    return true;
  }

  void reset() {
    initialized_ = false;
    last_associated_ = false;
    last_has_ip_ = false;
    since_ = 0U;
  }

 private:
  bool initialized_ = false;
  bool last_associated_ = false;
  bool last_has_ip_ = false;
  uint32_t since_ = 0U;
};

}  // namespace usage

#endif  // CODEX_USAGE_CONNECTION_WATCHDOG_H
