#include <stdint.h>
#include <unity.h>

#include "boot-button.h"

namespace {

uint32_t add(uint32_t value, uint32_t delta) { return static_cast<uint32_t>(value + delta); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_initially_held_button_has_no_spurious_press() {
  BootButton button;
  button.begin(true, 100U);

  TEST_ASSERT_FALSE(button.update(true, 1000U));
  TEST_ASSERT_TRUE(button.pressed());
  TEST_ASSERT_FALSE(button.update(false, 2000U));
  TEST_ASSERT_FALSE(button.update(false, 2034U));
  TEST_ASSERT_TRUE(button.pressed());
  TEST_ASSERT_FALSE(button.update(false, 2035U));
  TEST_ASSERT_FALSE(button.pressed());
  TEST_ASSERT_FALSE(button.update(true, 2040U));
  TEST_ASSERT_TRUE(button.update(true, 2075U));
}

void test_press_requires_debounce_boundary_and_does_not_repeat() {
  BootButton button;
  button.begin(false, 0U);

  TEST_ASSERT_FALSE(button.update(true, 0U));
  TEST_ASSERT_FALSE(button.update(true, 34U));
  TEST_ASSERT_TRUE(button.update(true, 35U));
  TEST_ASSERT_FALSE(button.update(true, 1000U));
}

void test_bounce_restarts_debounce_window() {
  BootButton button;
  button.begin(false, 0U);

  TEST_ASSERT_FALSE(button.update(true, 0U));
  TEST_ASSERT_FALSE(button.update(false, 10U));
  TEST_ASSERT_FALSE(button.update(true, 20U));
  TEST_ASSERT_FALSE(button.update(true, 54U));
  TEST_ASSERT_TRUE(button.update(true, 55U));
}

void test_release_then_press_is_a_new_click() {
  BootButton button;
  button.begin(false, 0U);

  TEST_ASSERT_FALSE(button.update(true, 35U));
  TEST_ASSERT_FALSE(button.update(false, 36U));
  TEST_ASSERT_FALSE(button.update(false, 70U));
  TEST_ASSERT_FALSE(button.pressed());
  TEST_ASSERT_FALSE(button.update(true, 71U));
  TEST_ASSERT_TRUE(button.update(true, 106U));
}

void test_debounce_is_wrap_safe() {
  const uint32_t base = 0xFFFFFFF0U;
  BootButton button;
  button.begin(false, base);

  TEST_ASSERT_FALSE(button.update(true, base));
  TEST_ASSERT_FALSE(button.update(true, add(base, 34U)));
  TEST_ASSERT_TRUE(button.update(true, add(base, 35U)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_initially_held_button_has_no_spurious_press);
  RUN_TEST(test_press_requires_debounce_boundary_and_does_not_repeat);
  RUN_TEST(test_bounce_restarts_debounce_window);
  RUN_TEST(test_release_then_press_is_a_new_click);
  RUN_TEST(test_debounce_is_wrap_safe);
  return UNITY_END();
}
