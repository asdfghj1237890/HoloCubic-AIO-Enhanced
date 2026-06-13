#include <unity.h>

#include "app/stockmarket/stockmarket_color_rule.h"

void setUp() {}
void tearDown() {}

void test_color_rule_parser_accepts_supported_rules()
{
    StockmarketColorRule rule = STOCKMARKET_COLOR_RULE_UP_GREEN;
    TEST_ASSERT_TRUE(stockmarket_color_rule_from_string("UP_GREEN", &rule));
    TEST_ASSERT_EQUAL(STOCKMARKET_COLOR_RULE_UP_GREEN, rule);

    TEST_ASSERT_TRUE(stockmarket_color_rule_from_string("UP_RED", &rule));
    TEST_ASSERT_EQUAL(STOCKMARKET_COLOR_RULE_UP_RED, rule);
}

void test_color_rule_parser_rejects_unknown_rules()
{
    StockmarketColorRule rule = STOCKMARKET_COLOR_RULE_UP_GREEN;
    TEST_ASSERT_FALSE(stockmarket_color_rule_from_string("BLUE", &rule));
    TEST_ASSERT_EQUAL(STOCKMARKET_COLOR_RULE_UP_GREEN, rule);
}

void test_up_green_rule_maps_up_to_green_and_down_to_red()
{
    StockmarketRgbColor up = stockmarket_direction_color(STOCKMARKET_COLOR_RULE_UP_GREEN, true);
    TEST_ASSERT_TRUE(up.on);
    TEST_ASSERT_EQUAL_UINT8(0, up.r);
    TEST_ASSERT_EQUAL_UINT8(255, up.g);
    TEST_ASSERT_EQUAL_UINT8(68, up.b);

    StockmarketRgbColor down = stockmarket_direction_color(STOCKMARKET_COLOR_RULE_UP_GREEN, false);
    TEST_ASSERT_TRUE(down.on);
    TEST_ASSERT_EQUAL_UINT8(255, down.r);
    TEST_ASSERT_EQUAL_UINT8(32, down.g);
    TEST_ASSERT_EQUAL_UINT8(32, down.b);
}

void test_up_red_rule_maps_up_to_red_and_down_to_green()
{
    StockmarketRgbColor up = stockmarket_direction_color(STOCKMARKET_COLOR_RULE_UP_RED, true);
    TEST_ASSERT_TRUE(up.on);
    TEST_ASSERT_EQUAL_UINT8(255, up.r);
    TEST_ASSERT_EQUAL_UINT8(32, up.g);
    TEST_ASSERT_EQUAL_UINT8(32, up.b);

    StockmarketRgbColor down = stockmarket_direction_color(STOCKMARKET_COLOR_RULE_UP_RED, false);
    TEST_ASSERT_TRUE(down.on);
    TEST_ASSERT_EQUAL_UINT8(0, down.r);
    TEST_ASSERT_EQUAL_UINT8(255, down.g);
    TEST_ASSERT_EQUAL_UINT8(68, down.b);
}

void test_led_uses_dimmed_colors_when_market_is_open()
{
    StockmarketRgbColor up = stockmarket_led_color(STOCKMARKET_COLOR_RULE_UP_GREEN, true, true);
    TEST_ASSERT_TRUE(up.on);
    TEST_ASSERT_EQUAL_UINT8(0, up.r);
    TEST_ASSERT_EQUAL_UINT8(96, up.g);
    TEST_ASSERT_EQUAL_UINT8(0, up.b);

    StockmarketRgbColor down = stockmarket_led_color(STOCKMARKET_COLOR_RULE_UP_GREEN, true, false);
    TEST_ASSERT_TRUE(down.on);
    TEST_ASSERT_EQUAL_UINT8(96, down.r);
    TEST_ASSERT_EQUAL_UINT8(0, down.g);
    TEST_ASSERT_EQUAL_UINT8(0, down.b);
}

void test_led_is_off_when_market_is_closed()
{
    StockmarketRgbColor closed = stockmarket_led_color(STOCKMARKET_COLOR_RULE_UP_GREEN, false, true);
    TEST_ASSERT_FALSE(closed.on);
    TEST_ASSERT_EQUAL_UINT8(0, closed.r);
    TEST_ASSERT_EQUAL_UINT8(0, closed.g);
    TEST_ASSERT_EQUAL_UINT8(0, closed.b);
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_color_rule_parser_accepts_supported_rules);
    RUN_TEST(test_color_rule_parser_rejects_unknown_rules);
    RUN_TEST(test_up_green_rule_maps_up_to_green_and_down_to_red);
    RUN_TEST(test_up_red_rule_maps_up_to_red_and_down_to_green);
    RUN_TEST(test_led_uses_dimmed_colors_when_market_is_open);
    RUN_TEST(test_led_is_off_when_market_is_closed);
    return UNITY_END();
}
