#include <unity.h>

#include "app/tomato/tomato_calc.h"

void setUp() {}
void tearDown() {}

void test_total_seconds()
{
    TEST_ASSERT_EQUAL_INT(1500, tomato_total_seconds(25, 0));
    TEST_ASSERT_EQUAL_INT(59, tomato_total_seconds(0, 59));
}

void test_progress_starts_at_zero()
{
    TEST_ASSERT_EQUAL_INT(0, tomato_progress_pct(1500, 1500));
}

void test_progress_midway()
{
    TEST_ASSERT_EQUAL_INT(50, tomato_progress_pct(1500, 750));
}

void test_progress_complete()
{
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(1500, 0));
}

void test_progress_zero_total_is_full()
{
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(0, 0));
}

void test_progress_clamps_out_of_range_remain()
{
    TEST_ASSERT_EQUAL_INT(0, tomato_progress_pct(1500, 2000)); // remain > total
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(1500, -5)); // negative remain
}

void test_focus_modes()
{
    TEST_ASSERT_TRUE(tomato_is_focus(0));
    TEST_ASSERT_TRUE(tomato_is_focus(1));
    TEST_ASSERT_FALSE(tomato_is_focus(-1));
    TEST_ASSERT_FALSE(tomato_is_focus(2));
}

void test_next_segment_minutes()
{
    TEST_ASSERT_EQUAL_INT(5, tomato_next_minutes(0));   // focus 25 -> break 5
    TEST_ASSERT_EQUAL_INT(15, tomato_next_minutes(1));  // focus 45 -> break 15
    TEST_ASSERT_EQUAL_INT(25, tomato_next_minutes(-1)); // break 5 -> focus 25
    TEST_ASSERT_EQUAL_INT(45, tomato_next_minutes(2));  // break 15 -> focus 45
    TEST_ASSERT_EQUAL_INT(5, tomato_next_minutes(7));   // out-of-range fallback
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_total_seconds);
    RUN_TEST(test_progress_starts_at_zero);
    RUN_TEST(test_progress_midway);
    RUN_TEST(test_progress_complete);
    RUN_TEST(test_progress_zero_total_is_full);
    RUN_TEST(test_progress_clamps_out_of_range_remain);
    RUN_TEST(test_focus_modes);
    RUN_TEST(test_next_segment_minutes);
    return UNITY_END();
}
