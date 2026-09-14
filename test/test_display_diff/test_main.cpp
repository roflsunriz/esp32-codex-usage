#include <array>
#include <unity.h>
#include "display-diff.h"

void setUp() {}
void tearDown() {}

void test_only_changed_bands_are_sent() {
  std::array<uint8_t, display_diff::kWidth * display_diff::kHeight> frame{};
  display_diff::Bands bands;
  TEST_ASSERT_EQUAL_HEX16(0x7FFF, bands.update(frame.data()));
  TEST_ASSERT_EQUAL_HEX16(0, bands.update(frame.data()));
  frame[20 * display_diff::kWidth + 7] = 3;
  TEST_ASSERT_EQUAL_HEX16(1U << 1, bands.update(frame.data()));
  frame[239 * display_diff::kWidth + 319] = 4;
  TEST_ASSERT_EQUAL_HEX16(1U << 14, bands.update(frame.data()));
  bands.invalidate();
  TEST_ASSERT_EQUAL_HEX16(0x7FFF, bands.update(frame.data()));
}

void test_adjacent_bands_are_one_transfer() {
  size_t count = 0;
  const bool success = display_diff::eachRun(
      static_cast<uint16_t>((1U << 2) | (1U << 3) | (1U << 8)),
      [&](size_t top, size_t height) {
        if (count == 0) { TEST_ASSERT_EQUAL_UINT32(32, top); TEST_ASSERT_EQUAL_UINT32(32, height); }
        if (count == 1) { TEST_ASSERT_EQUAL_UINT32(128, top); TEST_ASSERT_EQUAL_UINT32(16, height); }
        ++count; return true;
      });
  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_EQUAL_UINT32(2, count);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_only_changed_bands_are_sent);
  RUN_TEST(test_adjacent_bands_are_one_transfer);
  return UNITY_END();
}
