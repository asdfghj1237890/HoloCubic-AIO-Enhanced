#include "stockmarket_yahoo_price.h"

#include <stddef.h>
#include <time.h>

static bool stockmarket_yahoo_positive_price(float price)
{
    return price > 0.0f;
}

static bool stockmarket_yahoo_in_period(long value, long start, long end)
{
    return start > 0 && end > start && value >= start && value < end;
}

static bool stockmarket_yahoo_latest_is_live_for_period(
    const StockmarketYahooMeta *meta,
    long start,
    long end)
{
    return stockmarket_yahoo_positive_price(meta->latest_price) &&
           stockmarket_yahoo_in_period(meta->latest_timestamp, start, end);
}

static long stockmarket_yahoo_regular_quote_time(const StockmarketYahooMeta *meta)
{
    if (meta->regular_market_time > 0)
    {
        return meta->regular_market_time;
    }
    return meta->latest_timestamp;
}

static bool stockmarket_yahoo_latest_is_newer_than_regular(
    const StockmarketYahooMeta *meta)
{
    return stockmarket_yahoo_positive_price(meta->latest_price) &&
           meta->latest_timestamp > stockmarket_yahoo_regular_quote_time(meta);
}

StockmarketYahooSelection stockmarket_yahoo_select_price(
    const StockmarketYahooMeta *meta,
    long now_epoch)
{
    StockmarketYahooSelection selection = {0.0f, 0, false, STOCKMARKET_YAHOO_SESSION_CLOSED};
    if (NULL == meta)
    {
        return selection;
    }

    selection.price = meta->regular_price;
    selection.quote_timestamp = stockmarket_yahoo_regular_quote_time(meta);

    if (stockmarket_yahoo_in_period(now_epoch, meta->pre_start, meta->pre_end))
    {
        if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->pre_start, meta->pre_end))
        {
            selection.price = meta->latest_price;
            selection.quote_timestamp = meta->latest_timestamp;
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_PRE;
        }
        return selection;
    }

    if (stockmarket_yahoo_in_period(now_epoch, meta->regular_start, meta->regular_end))
    {
        if (stockmarket_yahoo_positive_price(meta->regular_price))
        {
            selection.quote_timestamp = stockmarket_yahoo_regular_quote_time(meta);
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_REGULAR;
            return selection;
        }

        if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->regular_start, meta->regular_end))
        {
            selection.price = meta->latest_price;
            selection.quote_timestamp = meta->latest_timestamp;
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_REGULAR;
        }
        return selection;
    }

    if (stockmarket_yahoo_in_period(now_epoch, meta->post_start, meta->post_end))
    {
        if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->post_start, meta->post_end))
        {
            selection.price = meta->latest_price;
            selection.quote_timestamp = meta->latest_timestamp;
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_POST;
        }
        return selection;
    }

    if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->post_start, meta->post_end))
    {
        selection.price = meta->latest_price;
        selection.quote_timestamp = meta->latest_timestamp;
    }
    else if (stockmarket_yahoo_latest_is_newer_than_regular(meta))
    {
        selection.price = meta->latest_price;
        selection.quote_timestamp = meta->latest_timestamp;
    }

    return selection;
}

const char *stockmarket_yahoo_session_to_string(StockmarketYahooSession session)
{
    switch (session)
    {
    case STOCKMARKET_YAHOO_SESSION_PRE:
        return "PRE";
    case STOCKMARKET_YAHOO_SESSION_REGULAR:
        return "REGULAR";
    case STOCKMARKET_YAHOO_SESSION_POST:
        return "POST";
    case STOCKMARKET_YAHOO_SESSION_CLOSED:
    default:
        return "CLOSED";
    }
}

static bool stockmarket_yahoo_gmtime(const time_t *timep, struct tm *out)
{
#if defined(_WIN32) && !defined(ARDUINO)
    return 0 == gmtime_s(out, timep);
#else
    return NULL != gmtime_r(timep, out);
#endif
}

bool stockmarket_yahoo_format_exchange_datetime(char *out,
                                                size_t out_len,
                                                long epoch,
                                                long gmtoffset)
{
    if (NULL == out || 0 == out_len || epoch <= 0)
    {
        return false;
    }

    out[0] = '\0';
    time_t adjusted = (time_t)(epoch + gmtoffset);
    struct tm timeinfo;
    if (!stockmarket_yahoo_gmtime(&adjusted, &timeinfo))
    {
        return false;
    }

    return strftime(out, out_len, "%H:%M", &timeinfo) > 0;
}
