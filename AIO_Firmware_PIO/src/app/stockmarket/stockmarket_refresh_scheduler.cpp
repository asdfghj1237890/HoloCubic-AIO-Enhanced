#include "stockmarket_refresh_scheduler.h"

#include <stddef.h>
#include <stdint.h>

static bool stockmarket_millis_reached(unsigned long now_millis,
                                       unsigned long due_millis)
{
    return (int32_t)((uint32_t)now_millis - (uint32_t)due_millis) >= 0;
}

static uint32_t stockmarket_millis_elapsed(unsigned long now_millis,
                                           unsigned long since_millis)
{
    return (uint32_t)now_millis - (uint32_t)since_millis;
}

void stockmarket_refresh_scheduler_init(StockmarketRefreshScheduler *scheduler,
                                        unsigned long now_millis)
{
    if (NULL == scheduler)
    {
        return;
    }

    scheduler->next_due_millis = (uint32_t)now_millis;
    scheduler->in_flight_since_millis = 0;
    scheduler->in_flight = false;
}

bool stockmarket_refresh_scheduler_begin_if_due(StockmarketRefreshScheduler *scheduler,
                                                unsigned long now_millis)
{
    if (NULL == scheduler || scheduler->in_flight ||
        !stockmarket_millis_reached(now_millis, scheduler->next_due_millis))
    {
        return false;
    }

    scheduler->in_flight = true;
    scheduler->in_flight_since_millis = (uint32_t)now_millis;
    return true;
}

void stockmarket_refresh_scheduler_finish(StockmarketRefreshScheduler *scheduler,
                                          unsigned long now_millis,
                                          unsigned long interval_millis)
{
    if (NULL == scheduler || !scheduler->in_flight)
    {
        return;
    }

    if (0 == interval_millis)
    {
        interval_millis = 1;
    }

    do
    {
        scheduler->next_due_millis =
            (uint32_t)(scheduler->next_due_millis + interval_millis);
    } while (stockmarket_millis_reached(now_millis, scheduler->next_due_millis));

    scheduler->in_flight = false;
    scheduler->in_flight_since_millis = 0;
}

bool stockmarket_refresh_scheduler_expire_in_flight(StockmarketRefreshScheduler *scheduler,
                                                    unsigned long now_millis,
                                                    unsigned long timeout_millis)
{
    if (NULL == scheduler || !scheduler->in_flight)
    {
        return false;
    }

    if (stockmarket_millis_elapsed(now_millis,
                                   scheduler->in_flight_since_millis) <
        timeout_millis)
    {
        return false;
    }

    scheduler->in_flight = false;
    scheduler->in_flight_since_millis = 0;
    scheduler->next_due_millis = (uint32_t)now_millis;
    return true;
}
