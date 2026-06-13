#include "stockmarket_yahoo_price.h"

#include <stddef.h>

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

StockmarketYahooSelection stockmarket_yahoo_select_price(
    const StockmarketYahooMeta *meta,
    long now_epoch)
{
    StockmarketYahooSelection selection = {0.0f, false, STOCKMARKET_YAHOO_SESSION_CLOSED};
    if (NULL == meta)
    {
        return selection;
    }

    selection.price = meta->regular_price;

    if (stockmarket_yahoo_in_period(now_epoch, meta->pre_start, meta->pre_end))
    {
        if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->pre_start, meta->pre_end))
        {
            selection.price = meta->latest_price;
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_PRE;
        }
        return selection;
    }

    if (stockmarket_yahoo_in_period(now_epoch, meta->regular_start, meta->regular_end))
    {
        if (stockmarket_yahoo_positive_price(meta->regular_price))
        {
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_REGULAR;
            return selection;
        }

        if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->regular_start, meta->regular_end))
        {
            selection.price = meta->latest_price;
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
            selection.market_active = true;
            selection.session = STOCKMARKET_YAHOO_SESSION_POST;
        }
        return selection;
    }

    if (stockmarket_yahoo_latest_is_live_for_period(meta, meta->post_start, meta->post_end))
    {
        selection.price = meta->latest_price;
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
