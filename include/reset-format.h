#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

namespace usage {

// wham/usage の reset_at は秒単位の UNIX 時刻(UTC)である。公式 codex-rs の
// backend-client (map_rate_limit_window) も reset_at をそのまま resetsAt と
// して扱い、reset_after_seconds は表示に使わない。ESP32 の時計は NTP 同期の
// UTC なので、表示だけ JST(UTC+9)へ寄せる。
constexpr int64_t kResetDisplayJstOffsetSeconds = 9 * 3600;
// これより前の時刻は NTP 未同期とみなす (2025-01-01 00:00:00 UTC)。
constexpr int64_t kResetClockReadyEpoch = 1735689600;
constexpr int64_t kResetDaySeconds = 86400;

// JST の「M/D HH:MM」。reset_at がなければ "--"。
inline std::string resetDateText(int64_t resetsAt) {
  if (resetsAt <= 0) return "--";
  const time_t moment = static_cast<time_t>(resetsAt + kResetDisplayJstOffsetSeconds);
  std::tm fields{};
#if defined(_WIN32)
  if (gmtime_s(&fields, &moment) != 0) return "--";
#else
  if (!gmtime_r(&moment, &fields)) return "--";
#endif
  char text[16];
  std::snprintf(text, sizeof(text), "%d/%d %02d:%02d", fields.tm_mon + 1, fields.tm_mday,
                fields.tm_hour, fields.tm_min);
  return text;
}

// 基本は「あとX日Y時間」、24 時間未満だけ「あとX時間Y分」(1 時間未満は分だけ)。
// reset_at 不明・時計未同期は "--"、期限切れは次の取得待ちとして "更新待ち"。
inline std::string remainingText(int64_t resetsAt, int64_t nowEpoch) {
  if (resetsAt <= 0 || nowEpoch < kResetClockReadyEpoch) return "--";
  const int64_t rest = resetsAt - nowEpoch;
  if (rest <= 0) return "更新待ち";
  if (rest < 60) return "あと1分未満";
  if (rest < kResetDaySeconds) {
    const int64_t hours = rest / 3600;
    const int64_t minutes = (rest % 3600) / 60;
    if (hours <= 0) return "あと" + std::to_string(minutes) + "分";
    return "あと" + std::to_string(hours) + "時間" + std::to_string(minutes) + "分";
  }
  const int64_t days = rest / kResetDaySeconds;
  const int64_t hours = (rest % kResetDaySeconds) / 3600;
  return "あと" + std::to_string(days) + "日" + std::to_string(hours) + "時間";
}

// 使用量タブの 1 行表示。「リセット 9/15 18:11 あと2日5時間」など。
inline std::string resetLineText(int64_t resetsAt, int64_t nowEpoch) {
  if (resetsAt <= 0) return "リセット --";
  std::string line = "リセット " + resetDateText(resetsAt);
  const std::string rest = remainingText(resetsAt, nowEpoch);
  if (rest != "--") {
    line += " ";
    line += rest;
  }
  return line;
}

}  // namespace usage
