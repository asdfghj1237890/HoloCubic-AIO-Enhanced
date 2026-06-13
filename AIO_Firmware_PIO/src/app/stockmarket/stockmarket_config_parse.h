#ifndef AIO_STOCKMARKET_CONFIG_PARSE_H
#define AIO_STOCKMARKET_CONFIG_PARSE_H

#include <stddef.h>

#include "stockmarket_color_rule.h"

#define STOCKMARKET_DEFAULT_SYMBOL "AAPL"
#define STOCKMARKET_DEFAULT_MARKET "US"
#define STOCKMARKET_DEFAULT_INTERVAL 10000UL
#define STOCKMARKET_DEFAULT_COLOR_RULE STOCKMARKET_COLOR_RULE_UP_GREEN_TEXT

struct StockmarketRawConfig
{
    char stock_symbol[32];
    char market_type[4];
    unsigned long updataInterval;
    char color_rule[12];
};

void stockmarket_default_config(StockmarketRawConfig *out);

// Parse /stockmarket.cfg into bounded raw fields. Returns false when the
// caller should rewrite the file with defaults.
bool stockmarket_parse_config(char *buffer_mut,
                              size_t size,
                              bool truncated,
                              StockmarketRawConfig *out);

#endif
