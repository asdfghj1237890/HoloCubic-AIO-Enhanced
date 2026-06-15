#include <unity.h>

#include "app/stockmarket/stockmarket_refresh_scheduler.h"

void test_refresh_scheduler_requests_immediately_then_blocks_duplicates()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 1000UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1000UL));
    TEST_ASSERT_FALSE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1001UL));
}

void test_refresh_scheduler_keeps_fixed_slots_after_slow_fetch()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 1000UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1000UL));
    stockmarket_refresh_scheduler_finish(&scheduler, 1300UL, 1000UL);

    TEST_ASSERT_FALSE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1999UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 2000UL));
}

void test_refresh_scheduler_skips_missed_slots_after_very_slow_fetch()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 1000UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1000UL));
    stockmarket_refresh_scheduler_finish(&scheduler, 3500UL, 1000UL);

    TEST_ASSERT_FALSE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 3999UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 4000UL));
}

void test_refresh_scheduler_timeout_releases_stuck_request()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 1000UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1000UL));
    TEST_ASSERT_FALSE(stockmarket_refresh_scheduler_expire_in_flight(
        &scheduler, 2999UL, 2000UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_expire_in_flight(
        &scheduler, 3000UL, 2000UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 3000UL));
}

void test_refresh_scheduler_timeout_handles_millis_wrap()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 0xFFFFFF00UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(
        &scheduler, 0xFFFFFF00UL));
    TEST_ASSERT_FALSE(stockmarket_refresh_scheduler_expire_in_flight(
        &scheduler, 244UL, 1000UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_expire_in_flight(
        &scheduler, 744UL, 1000UL));
    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 744UL));
}

void test_refresh_scheduler_finish_without_in_flight_does_not_skip_slot()
{
    StockmarketRefreshScheduler scheduler;
    stockmarket_refresh_scheduler_init(&scheduler, 1000UL);

    stockmarket_refresh_scheduler_finish(&scheduler, 1500UL, 1000UL);

    TEST_ASSERT_TRUE(stockmarket_refresh_scheduler_begin_if_due(&scheduler, 1500UL));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_refresh_scheduler_requests_immediately_then_blocks_duplicates);
    RUN_TEST(test_refresh_scheduler_keeps_fixed_slots_after_slow_fetch);
    RUN_TEST(test_refresh_scheduler_skips_missed_slots_after_very_slow_fetch);
    RUN_TEST(test_refresh_scheduler_timeout_releases_stuck_request);
    RUN_TEST(test_refresh_scheduler_timeout_handles_millis_wrap);
    RUN_TEST(test_refresh_scheduler_finish_without_in_flight_does_not_skip_slot);
    return UNITY_END();
}
