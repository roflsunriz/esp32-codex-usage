#pragma once
#include <ArduinoJson.h>

#include <cmath>
#include <cstdint>

namespace usage {
struct Window {
  bool available = false;
  float used = 0;
  int64_t resetsAt = 0;
};
struct Reading {
  Window fiveHour;
  Window weekly;
};

// Map by duration: primary/secondary order alone does not define the period.
inline bool parseUsage(JsonVariantConst document, Reading& result) {
  if (!document.is<JsonObjectConst>() || !document["plan_type"].is<const char*>()) return false;
  Reading next;
  JsonVariantConst limit = document["rate_limit"];
  if (!limit.isNull() && !limit.is<JsonObjectConst>()) return false;
  for (const char* name : {"primary_window", "secondary_window"}) {
    JsonVariantConst value = limit[name];
    if (value.isNull()) continue;
    if (!value.is<JsonObjectConst>() || !value["limit_window_seconds"].is<int>() ||
        !value["used_percent"].is<float>())
      return false;
    const float percent = value["used_percent"].as<float>();
    if (!std::isfinite(percent) || percent < 0) return false;
    const int seconds = value["limit_window_seconds"].as<int>();
    Window* target = seconds == 18000 ? &next.fiveHour : seconds == 604800 ? &next.weekly : nullptr;
    if (!target) continue;
    if (target->available) return false;
    target->available = true;
    target->used = percent > 100 ? 100 : percent;
    if (!value["reset_at"].isNull()) {
      if (!value["reset_at"].is<int64_t>() || value["reset_at"].as<int64_t>() < 0) return false;
      target->resetsAt = value["reset_at"].as<int64_t>();
    }
  }
  result = next;
  return true;
}
}  // namespace usage
