#include <unity.h>

#include "app/stockmarket/stockmarket_yahoo_price.h"

static StockmarketYahooMeta base_meta()
{
    StockmarketYahooMeta meta = {};
    meta.regular_price = 100.0f;
    meta.previous_close = 98.0f;
    meta.latest_price = 0.0f;
    meta.latest_timestamp = 0;
    meta.pre_start = 1000;
    meta.pre_end = 2000;
    meta.regular_start = 2000;
    meta.regular_end = 3000;
    meta.post_start = 3000;
    meta.post_end = 4000;
    return meta;
}

void test_premarket_uses_latest_close_when_timestamp_is_in_pre_session()
{
    StockmarketYahooMeta meta = base_meta();
    meta.latest_price = 103.5f;
    meta.latest_timestamp = 1500;

    StockmarketYahooSelection selection =
        stockmarket_yahoo_select_price(&meta, 1600);

    TEST_ASSERT_EQUAL(STOCKMARKET_YAHOO_SESSION_PRE, selection.session);
    TEST_ASSERT_TRUE(selection.market_active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 103.5f, selection.price);
}

void test_afterhours_uses_latest_close_when_timestamp_is_in_post_session()
{
    StockmarketYahooMeta meta = base_meta();
    meta.latest_price = 96.25f;
    meta.latest_timestamp = 3500;

    StockmarketYahooSelection selection =
        stockmarket_yahoo_select_price(&meta, 3600);

    TEST_ASSERT_EQUAL(STOCKMARKET_YAHOO_SESSION_POST, selection.session);
    TEST_ASSERT_TRUE(selection.market_active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 96.25f, selection.price);
}

void test_regular_session_uses_regular_market_price()
{
    StockmarketYahooMeta meta = base_meta();
    meta.latest_price = 101.25f;
    meta.latest_timestamp = 2500;

    StockmarketYahooSelection selection =
        stockmarket_yahoo_select_price(&meta, 2500);

    TEST_ASSERT_EQUAL(STOCKMARKET_YAHOO_SESSION_REGULAR, selection.session);
    TEST_ASSERT_TRUE(selection.market_active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, selection.price);
}

void test_closed_afterhours_keeps_last_postmarket_price_but_inactive_led()
{
    StockmarketYahooMeta meta = base_meta();
    meta.latest_price = 96.25f;
    meta.latest_timestamp = 3500;

    StockmarketYahooSelection selection =
        stockmarket_yahoo_select_price(&meta, 4500);

    TEST_ASSERT_EQUAL(STOCKMARKET_YAHOO_SESSION_CLOSED, selection.session);
    TEST_ASSERT_FALSE(selection.market_active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 96.25f, selection.price);
}

void test_closed_market_falls_back_to_regular_price_when_latest_is_not_postmarket()
{
    StockmarketYahooMeta meta = base_meta();
    meta.latest_price = 102.0f;
    meta.latest_timestamp = 900;

    StockmarketYahooSelection selection =
        stockmarket_yahoo_select_price(&meta, 4500);

    TEST_ASSERT_EQUAL(STOCKMARKET_YAHOO_SESSION_CLOSED, selection.session);
    TEST_ASSERT_FALSE(selection.market_active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, selection.price);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_premarket_uses_latest_close_when_timestamp_is_in_pre_session);
    RUN_TEST(test_afterhours_uses_latest_close_when_timestamp_is_in_post_session);
    RUN_TEST(test_regular_session_uses_regular_market_price);
    RUN_TEST(test_closed_afterhours_keeps_last_postmarket_price_but_inactive_led);
    RUN_TEST(test_closed_market_falls_back_to_regular_price_when_latest_is_not_postmarket);
    return UNITY_END();
}
