#include <stddef.h>
#include <stdint.h>
#include <unity.h>

#include "display-state.h"

namespace {

const uint32_t kTimeoutOptions[] = {
    15000U, 30000U, 60000U, 120000U, 300000U, 600000U, 1800000U, 3600000U, 7200000U,
};

const size_t kTimeoutOptionCount = sizeof(kTimeoutOptions) / sizeof(kTimeoutOptions[0]);

uint32_t add(uint32_t value, uint32_t delta) { return static_cast<uint32_t>(value + delta); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_default_timeout_and_all_options_are_accepted() {
  DisplayState state(123U);

  TEST_ASSERT_TRUE(state.awake());
  TEST_ASSERT_EQUAL_UINT32(60000U, state.timeout());

  for (size_t i = 0; i < kTimeoutOptionCount; ++i) {
    TEST_ASSERT_TRUE(state.setTimeout(kTimeoutOptions[i], 456U));
    TEST_ASSERT_EQUAL_UINT32(kTimeoutOptions[i], state.timeout());
  }
}

void test_values_adjacent_to_every_option_are_rejected() {
  DisplayState state(0U);

  for (size_t i = 0; i < kTimeoutOptionCount; ++i) {
    const uint32_t option = kTimeoutOptions[i];
    TEST_ASSERT_FALSE(state.setTimeout(option - 1U, 999U));
    TEST_ASSERT_FALSE(state.setTimeout(option + 1U, 999U));
  }

  TEST_ASSERT_FALSE(state.setTimeout(0U, 999U));
  TEST_ASSERT_FALSE(state.setTimeout(0xFFFFFFFFU, 999U));
  TEST_ASSERT_EQUAL_UINT32(60000U, state.timeout());
}

void test_invalid_timeout_does_not_reset_activity_deadline() {
  DisplayState state(100U);

  TEST_ASSERT_TRUE(state.setTimeout(15000U, 100U));
  TEST_ASSERT_FALSE(state.setTimeout(1U, 0xFFFFFFFFU));

  state.tick(add(100U, 14999U));
  TEST_ASSERT_TRUE(state.awake());
  state.tick(add(100U, 15000U));
  TEST_ASSERT_FALSE(state.awake());
}

void test_timeout_boundary_is_wrap_safe() {
  const uint32_t base = 0xFFFFFF00U;
  DisplayState state(base);

  TEST_ASSERT_TRUE(state.setTimeout(15000U, base));

  state.tick(add(base, 14999U));
  TEST_ASSERT_TRUE(state.awake());
  state.tick(add(base, 15000U));
  TEST_ASSERT_FALSE(state.awake());
}

void test_touch_returns_only_new_actionable_presses() {
  DisplayState state(0U);

  TEST_ASSERT_FALSE(state.touch(false, 1U));
  TEST_ASSERT_TRUE(state.touch(true, 2U));
  TEST_ASSERT_FALSE(state.touch(true, 3U));
  TEST_ASSERT_FALSE(state.touch(false, 4U));
  TEST_ASSERT_TRUE(state.touch(true, 5U));
}

void test_first_gesture_after_auto_off_only_wakes_until_release() {
  DisplayState state(0U);
  TEST_ASSERT_TRUE(state.setTimeout(15000U, 0U));

  state.tick(15000U);
  TEST_ASSERT_FALSE(state.awake());

  TEST_ASSERT_FALSE(state.touch(true, 15001U));
  TEST_ASSERT_TRUE(state.awake());
  TEST_ASSERT_FALSE(state.touch(true, 15002U));
  TEST_ASSERT_FALSE(state.touch(false, 15003U));
  TEST_ASSERT_TRUE(state.touch(true, 15004U));
}

void test_touch_holding_across_auto_off_cannot_activate_a_control() {
  DisplayState state(0U);
  TEST_ASSERT_TRUE(state.setTimeout(15000U, 0U));

  TEST_ASSERT_TRUE(state.touch(true, 0U));
  TEST_ASSERT_FALSE(state.touch(true, 15000U));
  TEST_ASSERT_FALSE(state.awake());
  TEST_ASSERT_FALSE(state.touch(false, 15001U));
  TEST_ASSERT_FALSE(state.touch(true, 15002U));
  TEST_ASSERT_TRUE(state.awake());
  TEST_ASSERT_FALSE(state.touch(false, 15003U));
  TEST_ASSERT_TRUE(state.touch(true, 15004U));
}

void test_touch_at_deadline_wakes_instead_of_activating() {
  DisplayState state(0U);
  TEST_ASSERT_TRUE(state.setTimeout(15000U, 0U));

  TEST_ASSERT_TRUE(state.touch(true, 0U));
  TEST_ASSERT_FALSE(state.touch(false, 1U));

  TEST_ASSERT_FALSE(state.touch(true, 15000U));
  TEST_ASSERT_TRUE(state.awake());
  TEST_ASSERT_FALSE(state.touch(false, 15001U));
  TEST_ASSERT_TRUE(state.touch(true, 15002U));
}

void test_wake_restarts_timer_after_auto_off() {
  DisplayState state(0U);
  TEST_ASSERT_TRUE(state.setTimeout(15000U, 0U));
  state.tick(15000U);
  TEST_ASSERT_FALSE(state.awake());

  state.wake(20000U);
  TEST_ASSERT_TRUE(state.awake());
  state.tick(34999U);
  TEST_ASSERT_TRUE(state.awake());
  state.tick(35000U);
  TEST_ASSERT_FALSE(state.awake());
}

void test_wake_is_wrap_safe_and_consumes_held_touch() {
  const uint32_t base = 0xFFFFFF00U;
  DisplayState state(base);
  TEST_ASSERT_TRUE(state.setTimeout(15000U, base));
  TEST_ASSERT_TRUE(state.touch(true, base));
  state.tick(static_cast<uint32_t>(base + 15000U));
  TEST_ASSERT_FALSE(state.awake());

  state.wake(static_cast<uint32_t>(base + 15000U));
  TEST_ASSERT_FALSE(state.touch(true, static_cast<uint32_t>(base + 15001U)));
  TEST_ASSERT_FALSE(state.touch(false, static_cast<uint32_t>(base + 15002U)));
  TEST_ASSERT_TRUE(state.touch(true, static_cast<uint32_t>(base + 15003U)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_timeout_and_all_options_are_accepted);
  RUN_TEST(test_values_adjacent_to_every_option_are_rejected);
  RUN_TEST(test_invalid_timeout_does_not_reset_activity_deadline);
  RUN_TEST(test_timeout_boundary_is_wrap_safe);
  RUN_TEST(test_touch_returns_only_new_actionable_presses);
  RUN_TEST(test_first_gesture_after_auto_off_only_wakes_until_release);
  RUN_TEST(test_touch_holding_across_auto_off_cannot_activate_a_control);
  RUN_TEST(test_touch_at_deadline_wakes_instead_of_activating);
  RUN_TEST(test_wake_restarts_timer_after_auto_off);
  RUN_TEST(test_wake_is_wrap_safe_and_consumes_held_touch);
  return UNITY_END();
}
