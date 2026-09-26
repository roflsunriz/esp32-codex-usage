#include <stdint.h>
#include <unity.h>

#include "ui-tabs.h"

void setUp() {}
void tearDown() {}

// 認証済みでもコード待ち（再ログイン）は接続タブを残し、コード確認できる。
void test_connection_stays_while_device_code_pending() {
  TEST_ASSERT_EQUAL_UINT8(3U, ui_tabs::tabCount(false, false));
  TEST_ASSERT_EQUAL_UINT8(3U, ui_tabs::tabCount(false, true));
  TEST_ASSERT_EQUAL_UINT8(3U, ui_tabs::tabCount(true, true));
  TEST_ASSERT_EQUAL_UINT8(2U, ui_tabs::tabCount(true, false));
}

// コード待ちの間は接続タブに留まり、完了後にだけ使用量へ戻す。
void test_return_to_usage_only_after_code_cleared() {
  TEST_ASSERT_TRUE(ui_tabs::shouldReturnToUsage(true, false, 2U));
  TEST_ASSERT_FALSE(ui_tabs::shouldReturnToUsage(true, true, 2U));
  TEST_ASSERT_FALSE(ui_tabs::shouldReturnToUsage(false, false, 2U));
  TEST_ASSERT_FALSE(ui_tabs::shouldReturnToUsage(true, false, 0U));
  TEST_ASSERT_FALSE(ui_tabs::shouldReturnToUsage(true, false, 1U));
}

// 残り秒数は使用量取得の目安であり、認証コード待ちの促しに付けない。
void test_countdown_hidden_while_device_code_pending() {
  TEST_ASSERT_TRUE(ui_tabs::shouldShowCountdown(false, 300000U, false));
  TEST_ASSERT_FALSE(ui_tabs::shouldShowCountdown(false, 300000U, true));
  TEST_ASSERT_FALSE(ui_tabs::shouldShowCountdown(true, 300000U, false));
  TEST_ASSERT_FALSE(ui_tabs::shouldShowCountdown(false, 0U, false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_connection_stays_while_device_code_pending);
  RUN_TEST(test_return_to_usage_only_after_code_cleared);
  RUN_TEST(test_countdown_hidden_while_device_code_pending);
  return UNITY_END();
}
