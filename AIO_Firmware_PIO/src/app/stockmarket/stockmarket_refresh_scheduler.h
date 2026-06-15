#ifndef AIO_STOCKMARKET_REFRESH_SCHEDULER_H
#define AIO_STOCKMARKET_REFRESH_SCHEDULER_H

#include <stdbool.h>

typedef struct
{
    unsigned long next_due_millis;
    unsigned long in_flight_since_millis;
    bool in_flight;
} StockmarketRefreshScheduler;

#ifdef __cplusplus
extern "C" {
#endif

void stockmarket_refresh_scheduler_init(StockmarketRefreshScheduler *scheduler,
                                        unsigned long now_millis);

bool stockmarket_refresh_scheduler_begin_if_due(StockmarketRefreshScheduler *scheduler,
                                                unsigned long now_millis);

void stockmarket_refresh_scheduler_finish(StockmarketRefreshScheduler *scheduler,
                                          unsigned long now_millis,
                                          unsigned long interval_millis);

bool stockmarket_refresh_scheduler_expire_in_flight(StockmarketRefreshScheduler *scheduler,
                                                    unsigned long now_millis,
                                                    unsigned long timeout_millis);

#ifdef __cplusplus
}
#endif

#endif
