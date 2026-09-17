#include <unity.h>

#include "reset-format.h"

void setUp() {}
void tearDown() {}

void reset_date_is_jst_and_guards_invalid() {
  // 1789463468 = 2026-09-15 09:11 UTC → JST 9/15 18:11。
  TEST_ASSERT_EQUAL_STRING("9/15 18:11", usage::resetDateText(1789463468).c_str());
  TEST_ASSERT_EQUAL_STRING("--", usage::resetDateText(0).c_str());
  TEST_ASSERT_EQUAL_STRING("--", usage::resetDateText(-5).c_str());
}

void remaining_uses_days_hours_or_hours_minutes() {
  const int64_t now = 1789272668;  // reset の 2 日 5 時間前。
  const int64_t reset = 1789463468;
  TEST_ASSERT_EQUAL_STRING("あと2日5時間", usage::remainingText(reset, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと7日0時間",
                           usage::remainingText(now + 7 * 86400, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと1日0時間", usage::remainingText(now + 86400, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと23時間59分",
                           usage::remainingText(now + 86399, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと2時間20分",
                           usage::remainingText(now + 2 * 3600 + 20 * 60, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと5分", usage::remainingText(now + 300, now).c_str());
  TEST_ASSERT_EQUAL_STRING("あと1分未満", usage::remainingText(now + 30, now).c_str());
}

void remaining_reports_unknown_and_expiry() {
  const int64_t now = 1789272668;
  const int64_t reset = 1789463468;
  TEST_ASSERT_EQUAL_STRING("--", usage::remainingText(0, now).c_str());
  TEST_ASSERT_EQUAL_STRING("--", usage::remainingText(reset, 1000000).c_str());
  TEST_ASSERT_EQUAL_STRING("更新待ち", usage::remainingText(reset, reset).c_str());
  TEST_ASSERT_EQUAL_STRING("更新待ち", usage::remainingText(reset, reset + 600).c_str());
}

void reset_line_combines_date_and_remaining() {
  const int64_t now = 1789272668;
  TEST_ASSERT_EQUAL_STRING("リセット 9/15 18:11 あと2日5時間",
                           usage::resetLineText(1789463468, now).c_str());
  TEST_ASSERT_EQUAL_STRING("リセット --", usage::resetLineText(0, now).c_str());
  TEST_ASSERT_EQUAL_STRING("リセット 9/15 18:11",
                           usage::resetLineText(1789463468, 1000000).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(reset_date_is_jst_and_guards_invalid);
  RUN_TEST(remaining_uses_days_hours_or_hours_minutes);
  RUN_TEST(remaining_reports_unknown_and_expiry);
  RUN_TEST(reset_line_combines_date_and_remaining);
  return UNITY_END();
}
