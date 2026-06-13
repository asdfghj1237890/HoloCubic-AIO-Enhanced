#include "stockmarket_color_rule.h"

#include <stddef.h>
#include <string.h>

static const StockmarketRgbColor STOCKMARKET_RGB_OFF = {false, 0, 0, 0};
static const StockmarketRgbColor STOCKMARKET_SCREEN_GREEN = {true, 0, 255, 68};
static const StockmarketRgbColor STOCKMARKET_SCREEN_RED = {true, 255, 32, 32};
static const StockmarketRgbColor STOCKMARKET_LED_GREEN = {true, 0, 96, 0};
static const StockmarketRgbColor STOCKMARKET_LED_RED = {true, 96, 0, 0};

static StockmarketRgbColor stockmarket_color_for_rule(StockmarketColorRule rule,
                                                      bool is_up,
                                                      StockmarketRgbColor green,
                                                      StockmarketRgbColor red)
{
    if (STOCKMARKET_COLOR_RULE_UP_RED == rule)
    {
        return is_up ? red : green;
    }

    return is_up ? green : red;
}

bool stockmarket_color_rule_from_string(const char *text,
                                        StockmarketColorRule *out)
{
    if (NULL == text || NULL == out)
    {
        return false;
    }

    if (0 == strcmp(text, STOCKMARKET_COLOR_RULE_UP_GREEN_TEXT))
    {
        *out = STOCKMARKET_COLOR_RULE_UP_GREEN;
        return true;
    }
    if (0 == strcmp(text, STOCKMARKET_COLOR_RULE_UP_RED_TEXT))
    {
        *out = STOCKMARKET_COLOR_RULE_UP_RED;
        return true;
    }

    return false;
}

const char *stockmarket_color_rule_to_string(StockmarketColorRule rule)
{
    switch (rule)
    {
    case STOCKMARKET_COLOR_RULE_UP_RED:
        return STOCKMARKET_COLOR_RULE_UP_RED_TEXT;
    case STOCKMARKET_COLOR_RULE_UP_GREEN:
    default:
        return STOCKMARKET_COLOR_RULE_UP_GREEN_TEXT;
    }
}

StockmarketRgbColor stockmarket_direction_color(StockmarketColorRule rule,
                                                bool is_up)
{
    return stockmarket_color_for_rule(rule, is_up,
                                      STOCKMARKET_SCREEN_GREEN,
                                      STOCKMARKET_SCREEN_RED);
}

StockmarketRgbColor stockmarket_led_color(StockmarketColorRule rule,
                                          bool market_open,
                                          bool is_up)
{
    if (!market_open)
    {
        return STOCKMARKET_RGB_OFF;
    }

    return stockmarket_color_for_rule(rule, is_up,
                                      STOCKMARKET_LED_GREEN,
                                      STOCKMARKET_LED_RED);
}
