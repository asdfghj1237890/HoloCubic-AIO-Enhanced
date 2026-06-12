#include <unity.h>
#include <string.h>

#include "Arduino.h"
#include "Wire.h"
#include "app/stockmarket/stockmarket_config_parse.h"

void setUp() {}
void tearDown() {}

static void assert_defaults(const StockmarketRawConfig &out)
{
    TEST_ASSERT_EQUAL_STRING("AAPL", out.stock_symbol);
    TEST_ASSERT_EQUAL_STRING("US", out.market_type);
    TEST_ASSERT_EQUAL_UINT32(10000UL, out.updataInterval);
}

void test_valid_stock_config_parses_all_fields()
{
    char buf[] = "0700\nHK\n15000\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("0700", out.stock_symbol);
    TEST_ASSERT_EQUAL_STRING("HK", out.market_type);
    TEST_ASSERT_EQUAL_UINT32(15000UL, out.updataInterval);
}

void test_empty_stock_config_requests_default_rewrite()
{
    char buf[1] = {0};
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, 0, false, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
}

void test_truncated_stock_config_requests_default_rewrite()
{
    char buf[] = "AAPL\nUS\n10000\nextra-bytes-that-did-not-fit";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), true, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
}

void test_missing_stock_config_lines_do_not_call_unsafe_splitter()
{
    char buf[] = "AAPL\nUS";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
}

void test_invalid_stock_market_requests_default_rewrite()
{
    char buf[] = "AAPL\nBAD\n10000\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
}

void test_invalid_stock_interval_requests_default_rewrite()
{
    char buf[] = "AAPL\nUS\n0\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_stock_config_parses_all_fields);
    RUN_TEST(test_empty_stock_config_requests_default_rewrite);
    RUN_TEST(test_truncated_stock_config_requests_default_rewrite);
    RUN_TEST(test_missing_stock_config_lines_do_not_call_unsafe_splitter);
    RUN_TEST(test_invalid_stock_market_requests_default_rewrite);
    RUN_TEST(test_invalid_stock_interval_requests_default_rewrite);
    return UNITY_END();
}
