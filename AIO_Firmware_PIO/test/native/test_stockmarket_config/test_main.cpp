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
    TEST_ASSERT_EQUAL_STRING("UP_GREEN", out.color_rule);
}

void test_valid_stock_config_parses_all_fields()
{
    char buf[] = "0700\nHK\n15000\nUP_RED\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("0700", out.stock_symbol);
    TEST_ASSERT_EQUAL_STRING("HK", out.market_type);
    TEST_ASSERT_EQUAL_UINT32(15000UL, out.updataInterval);
    TEST_ASSERT_EQUAL_STRING("UP_RED", out.color_rule);
}

void test_legacy_three_line_stock_config_keeps_stock_and_defaults_up_green()
{
    char buf[] = "MSFT\nUS\n30000\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("MSFT", out.stock_symbol);
    TEST_ASSERT_EQUAL_STRING("US", out.market_type);
    TEST_ASSERT_EQUAL_UINT32(30000UL, out.updataInterval);
    TEST_ASSERT_EQUAL_STRING("UP_GREEN", out.color_rule);
}

void test_invalid_stock_color_rule_requests_default_rewrite()
{
    char buf[] = "AAPL\nUS\n10000\nBLUE\n";
    StockmarketRawConfig out{};
    bool ok = stockmarket_parse_config(buf, strlen(buf), false, &out);
    TEST_ASSERT_FALSE(ok);
    assert_defaults(out);
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

void test_stock_interval_parser_bounds_runtime_values()
{
    unsigned long interval = 0;

    TEST_ASSERT_TRUE(stockmarket_parse_interval("1000", &interval));
    TEST_ASSERT_EQUAL_UINT32(1000UL, interval);

    TEST_ASSERT_TRUE(stockmarket_parse_interval("3600000", &interval));
    TEST_ASSERT_EQUAL_UINT32(3600000UL, interval);

    TEST_ASSERT_FALSE(stockmarket_parse_interval("999", &interval));
    TEST_ASSERT_FALSE(stockmarket_parse_interval("3600001", &interval));
    TEST_ASSERT_FALSE(stockmarket_parse_interval("", &interval));
    TEST_ASSERT_FALSE(stockmarket_parse_interval("1000ms", &interval));
    TEST_ASSERT_FALSE(stockmarket_parse_interval("1000", nullptr));
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_stock_config_parses_all_fields);
    RUN_TEST(test_legacy_three_line_stock_config_keeps_stock_and_defaults_up_green);
    RUN_TEST(test_invalid_stock_color_rule_requests_default_rewrite);
    RUN_TEST(test_empty_stock_config_requests_default_rewrite);
    RUN_TEST(test_truncated_stock_config_requests_default_rewrite);
    RUN_TEST(test_missing_stock_config_lines_do_not_call_unsafe_splitter);
    RUN_TEST(test_invalid_stock_market_requests_default_rewrite);
    RUN_TEST(test_invalid_stock_interval_requests_default_rewrite);
    RUN_TEST(test_stock_interval_parser_bounds_runtime_values);
    return UNITY_END();
}
