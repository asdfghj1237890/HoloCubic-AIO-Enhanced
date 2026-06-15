#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    STOCKMARKET_YAHOO_SESSION_CLOSED = 0,
    STOCKMARKET_YAHOO_SESSION_PRE,
    STOCKMARKET_YAHOO_SESSION_REGULAR,
    STOCKMARKET_YAHOO_SESSION_POST
} StockmarketYahooSession;

typedef struct
{
    float regular_price;
    float previous_close;
    float latest_price;
    long latest_timestamp;
    long regular_market_time;
    long pre_start;
    long pre_end;
    long regular_start;
    long regular_end;
    long post_start;
    long post_end;
    long gmtoffset;
} StockmarketYahooMeta;

typedef struct
{
    float price;
    long quote_timestamp;
    bool market_active;
    StockmarketYahooSession session;
} StockmarketYahooSelection;

#ifdef __cplusplus
extern "C" {
#endif

StockmarketYahooSelection stockmarket_yahoo_select_price(
    const StockmarketYahooMeta *meta,
    long now_epoch);

const char *stockmarket_yahoo_session_to_string(StockmarketYahooSession session);

bool stockmarket_yahoo_format_exchange_datetime(char *out,
                                                size_t out_len,
                                                long epoch,
                                                long gmtoffset);

#ifdef __cplusplus
}
#endif
