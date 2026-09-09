#include <stdint.h>
#include <unity.h>

#include "connection-watchdog.h"

namespace {

uint32_t add(uint32_t value, uint32_t delta) { return static_cast<uint32_t>(value + delta); }

}  // namespace

void test_unassociated_and_associated_deadlines_are_different() {
  usage::ConnectionWatchdog unassociated;
  TEST_ASSERT_FALSE(unassociated.tick(false, false, 100U));
  TEST_ASSERT_FALSE(unassociated.tick(false, false, 30100U - 1U));
  TEST_ASSERT_TRUE(unassociated.tick(false, false, 30100U));
  TEST_ASSERT_FALSE(unassociated.tick(false, false, 30101U));

  usage::ConnectionWatchdog associated;
  TEST_ASSERT_FALSE(associated.tick(true, false, 100U));
  TEST_ASSERT_FALSE(associated.tick(true, false, 120099U));
  TEST_ASSERT_TRUE(associated.tick(true, false, 120100U));
  TEST_ASSERT_FALSE(associated.tick(true, false, 120101U));
}

void test_link_transition_starts_a_new_grace_period() {
  usage::ConnectionWatchdog watchdog;

  TEST_ASSERT_FALSE(watchdog.tick(false, false, 0U));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 29999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 29999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 149998U));
  TEST_ASSERT_TRUE(watchdog.tick(true, false, 149999U));
}

void test_losing_ip_while_associated_starts_the_long_grace_period() {
  usage::ConnectionWatchdog watchdog;

  TEST_ASSERT_FALSE(watchdog.tick(true, true, 0U));
  TEST_ASSERT_FALSE(watchdog.tick(true, true, 500000U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 500000U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 619999U));
  TEST_ASSERT_TRUE(watchdog.tick(true, false, 620000U));
}

void test_regaining_ip_cancels_an_outstanding_deadline() {
  usage::ConnectionWatchdog watchdog;

  TEST_ASSERT_FALSE(watchdog.tick(true, false, 0U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 119999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, true, 119999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, true, 999999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 999999U));
  TEST_ASSERT_FALSE(watchdog.tick(true, false, 1119998U));
  TEST_ASSERT_TRUE(watchdog.tick(true, false, 1119999U));
}

void test_reset_starts_without_an_immediate_reconnect() {
  usage::ConnectionWatchdog watchdog;

  TEST_ASSERT_FALSE(watchdog.tick(false, false, 0U));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 29999U));
  watchdog.reset();
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 30000U));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 59999U));
  TEST_ASSERT_TRUE(watchdog.tick(false, false, 60000U));
}

void test_reconnect_request_is_not_repeated_every_tick() {
  usage::ConnectionWatchdog watchdog;

  TEST_ASSERT_FALSE(watchdog.tick(false, false, 0U));
  TEST_ASSERT_TRUE(watchdog.tick(false, false, 30000U));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 30001U));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, 59999U));
  TEST_ASSERT_TRUE(watchdog.tick(false, false, 60000U));
}

void test_deadline_is_wrap_safe() {
  usage::ConnectionWatchdog watchdog;
  const uint32_t base = 0xFFFFFF00U;

  TEST_ASSERT_FALSE(watchdog.tick(false, false, base));
  TEST_ASSERT_FALSE(watchdog.tick(false, false, add(base, 29999U)));
  TEST_ASSERT_TRUE(watchdog.tick(false, false, add(base, 30000U)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unassociated_and_associated_deadlines_are_different);
  RUN_TEST(test_link_transition_starts_a_new_grace_period);
  RUN_TEST(test_losing_ip_while_associated_starts_the_long_grace_period);
  RUN_TEST(test_regaining_ip_cancels_an_outstanding_deadline);
  RUN_TEST(test_reset_starts_without_an_immediate_reconnect);
  RUN_TEST(test_reconnect_request_is_not_repeated_every_tick);
  RUN_TEST(test_deadline_is_wrap_safe);
  return UNITY_END();
}
