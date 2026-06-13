#include "stockmarket_config_parse.h"
#include "driver/flash_fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void stockmarket_default_config(StockmarketRawConfig *out)
{
    if (NULL == out)
    {
        return;
    }

    snprintf(out->stock_symbol, sizeof(out->stock_symbol), "%s",
             STOCKMARKET_DEFAULT_SYMBOL);
    snprintf(out->market_type, sizeof(out->market_type), "%s",
             STOCKMARKET_DEFAULT_MARKET);
    out->updataInterval = STOCKMARKET_DEFAULT_INTERVAL;
    snprintf(out->color_rule, sizeof(out->color_rule), "%s",
             STOCKMARKET_DEFAULT_COLOR_RULE);
}

static bool has_required_lines(const char *buffer, size_t size, int lines)
{
    int count = 0;
    for (size_t i = 0; i < size; ++i)
    {
        if ('\n' == buffer[i])
        {
            ++count;
            if (count >= lines)
            {
                return true;
            }
        }
    }
    return false;
}

static bool valid_symbol(const char *symbol)
{
    size_t len = strlen(symbol);
    if (0 == len || len >= sizeof(((StockmarketRawConfig *)0)->stock_symbol))
    {
        return false;
    }

    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = (unsigned char)symbol[i];
        if (!isalnum(ch) && '.' != ch && '-' != ch && '_' != ch)
        {
            return false;
        }
    }
    return true;
}

static bool valid_market(const char *market)
{
    return 0 == strcmp(market, "US") ||
           0 == strcmp(market, "CN") ||
           0 == strcmp(market, "HK");
}

static bool parse_interval(const char *value, unsigned long *out)
{
    char *end = NULL;
    unsigned long interval = strtoul(value, &end, 10);
    if (end == value || '\0' != *end)
    {
        return false;
    }

    if (interval < 1000UL || interval > 3600000UL)
    {
        return false;
    }

    *out = interval;
    return true;
}

bool stockmarket_parse_config(char *buffer_mut,
                              size_t size,
                              bool truncated,
                              StockmarketRawConfig *out)
{
    if (NULL == out)
    {
        return false;
    }
    stockmarket_default_config(out);

    if (NULL == buffer_mut || 0 == size || truncated)
    {
        return false;
    }

    if (!has_required_lines(buffer_mut, size, 3))
    {
        return false;
    }

    const bool has_color_rule = has_required_lines(buffer_mut, size, 4);
    char *param[4] = {0};
    analyseParam(buffer_mut, has_color_rule ? 4 : 3, param);

    unsigned long interval = 0;
    StockmarketColorRule color_rule = STOCKMARKET_COLOR_RULE_UP_GREEN;
    if (!valid_symbol(param[0]) ||
        !valid_market(param[1]) ||
        !parse_interval(param[2], &interval) ||
        (has_color_rule &&
         !stockmarket_color_rule_from_string(param[3], &color_rule)))
    {
        return false;
    }

    snprintf(out->stock_symbol, sizeof(out->stock_symbol), "%s", param[0]);
    snprintf(out->market_type, sizeof(out->market_type), "%s", param[1]);
    out->updataInterval = interval;
    snprintf(out->color_rule, sizeof(out->color_rule), "%s",
             stockmarket_color_rule_to_string(color_rule));
    return true;
}
