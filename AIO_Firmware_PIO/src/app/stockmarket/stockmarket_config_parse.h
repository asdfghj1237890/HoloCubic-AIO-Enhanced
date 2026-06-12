#ifndef AIO_STOCKMARKET_CONFIG_PARSE_H
#define AIO_STOCKMARKET_CONFIG_PARSE_H

#include <stddef.h>

#define STOCKMARKET_DEFAULT_SYMBOL "AAPL"
#define STOCKMARKET_DEFAULT_MARKET "US"
#define STOCKMARKET_DEFAULT_INTERVAL 10000UL

struct StockmarketRawConfig
{
    char stock_symbol[32];
    char market_type[4];
    unsigned long updataInterval;
};

void stockmarket_default_config(StockmarketRawConfig *out);

// Parse /stockmarket.cfg into bounded raw fields. Returns false when the
// caller should rewrite the file with defaults.
bool stockmarket_parse_config(char *buffer_mut,
                              size_t size,
                              bool truncated,
                              StockmarketRawConfig *out);

#endif
