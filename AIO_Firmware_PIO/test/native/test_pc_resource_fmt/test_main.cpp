#include <unity.h>

#include "app/pc_resource/pc_resource_fmt.h"

void setUp() {}
void tearDown() {}

void test_speed_zero_is_kilobytes()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_STRING("0.0K", buf);
}

void test_speed_sub_megabyte_keeps_tenths()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 1005); // 100.5 KB/s
    TEST_ASSERT_EQUAL_STRING("100.5K", buf);
}

void test_speed_top_of_k_range()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 9999); // 999.9 KB/s
    TEST_ASSERT_EQUAL_STRING("999.9K", buf);
}

void test_speed_promotes_to_megabytes_at_1000k()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 10000); // 1000.0 KB/s
    TEST_ASSERT_EQUAL_STRING("1.0M", buf);
}

void test_speed_megabytes_keep_one_decimal()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 123465); // 12346.5 KB/s
    TEST_ASSERT_EQUAL_STRING("12.3M", buf);
}

void test_speed_negative_clamps_to_zero()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), -5);
    TEST_ASSERT_EQUAL_STRING("0.0K", buf);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_speed_zero_is_kilobytes);
    RUN_TEST(test_speed_sub_megabyte_keeps_tenths);
    RUN_TEST(test_speed_top_of_k_range);
    RUN_TEST(test_speed_promotes_to_megabytes_at_1000k);
    RUN_TEST(test_speed_megabytes_keep_one_decimal);
    RUN_TEST(test_speed_negative_clamps_to_zero);
    return UNITY_END();
}
