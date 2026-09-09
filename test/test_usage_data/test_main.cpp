#include <unity.h>

#include <limits>

#include "usage-data.h"

void setUp() {}
void tearDown() {}

void maps_by_duration_and_preserves_missing() {
  JsonDocument document;
  deserializeJson(
      document,
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"used_percent":21,"limit_window_seconds":604800,"reset_at":1789463468},"secondary_window":null}})");
  usage::Reading reading;
  TEST_ASSERT_TRUE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
  TEST_ASSERT_FALSE(reading.fiveHour.available);
  TEST_ASSERT_TRUE(reading.weekly.available);
  TEST_ASSERT_EQUAL_FLOAT(21, reading.weekly.used);
  TEST_ASSERT_EQUAL_INT64(1789463468, reading.weekly.resetsAt);
}

void maps_both_windows_and_zero() {
  JsonDocument document;
  deserializeJson(
      document,
      R"({"plan_type":"plus","rate_limit":{"primary_window":{"used_percent":0,"limit_window_seconds":18000},"secondary_window":{"used_percent":100,"limit_window_seconds":604800}}})");
  usage::Reading reading;
  TEST_ASSERT_TRUE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
  TEST_ASSERT_TRUE(reading.fiveHour.available);
  TEST_ASSERT_EQUAL_FLOAT(0, reading.fiveHour.used);
  TEST_ASSERT_EQUAL_FLOAT(100, reading.weekly.used);
}

void never_substitutes_other_model_or_period() {
  JsonDocument document;
  deserializeJson(
      document,
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"used_percent":50,"limit_window_seconds":3600}},"additional_rate_limits":[{"rate_limit":{"primary_window":{"used_percent":30,"limit_window_seconds":18000}}}]})");
  usage::Reading reading;
  TEST_ASSERT_TRUE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
  TEST_ASSERT_FALSE(reading.fiveHour.available);
  TEST_ASSERT_FALSE(reading.weekly.available);
}

void invalid_response_does_not_replace_last_reading() {
  const char* invalid[] = {
      "{}",
      "[]",
      R"({"plan_type":"pro","rate_limit":"bad"})",
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"limit_window_seconds":18000}}})",
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"limit_window_seconds":18000,"used_percent":"21"}}})",
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"limit_window_seconds":18000,"used_percent":-1}}})",
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"limit_window_seconds":18000,"used_percent":20,"reset_at":-1}}})",
      R"({"plan_type":"pro","rate_limit":{"primary_window":{"limit_window_seconds":18000,"used_percent":20},"secondary_window":{"limit_window_seconds":18000,"used_percent":30}}})"};
  for (const char* value : invalid) {
    JsonDocument document;
    deserializeJson(document, value);
    usage::Reading reading;
    reading.weekly.available = true;
    reading.weekly.used = 42;
    TEST_ASSERT_FALSE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
    TEST_ASSERT_TRUE(reading.weekly.available);
    TEST_ASSERT_EQUAL_FLOAT(42, reading.weekly.used);
  }
}

void null_limit_is_unavailable_not_zero() {
  JsonDocument document;
  deserializeJson(document, R"({"plan_type":"pro","rate_limit":null})");
  usage::Reading reading;
  reading.weekly.available = true;
  TEST_ASSERT_TRUE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
  TEST_ASSERT_FALSE(reading.weekly.available);
}

void clamps_over_limit_but_rejects_non_finite_values() {
  JsonDocument document;
  document["plan_type"] = "pro";
  document["rate_limit"]["primary_window"]["limit_window_seconds"] = 18000;
  document["rate_limit"]["primary_window"]["used_percent"] = 125.5;
  usage::Reading reading;
  TEST_ASSERT_TRUE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
  TEST_ASSERT_TRUE(reading.fiveHour.available);
  TEST_ASSERT_EQUAL_FLOAT(100, reading.fiveHour.used);
  for (float invalid :
       {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    document["rate_limit"]["primary_window"]["used_percent"] = invalid;
    TEST_ASSERT_FALSE(usage::parseUsage(document.as<JsonVariantConst>(), reading));
    TEST_ASSERT_EQUAL_FLOAT(100, reading.fiveHour.used);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(maps_by_duration_and_preserves_missing);
  RUN_TEST(maps_both_windows_and_zero);
  RUN_TEST(never_substitutes_other_model_or_period);
  RUN_TEST(invalid_response_does_not_replace_last_reading);
  RUN_TEST(null_limit_is_unavailable_not_zero);
  RUN_TEST(clamps_over_limit_but_rejects_non_finite_values);
  return UNITY_END();
}
