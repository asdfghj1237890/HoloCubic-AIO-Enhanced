#ifndef AIO_STUB_FREERTOS_TIMERS_H
#define AIO_STUB_FREERTOS_TIMERS_H
#include "FreeRTOS.h"

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);

static inline TimerHandle_t xTimerCreate(const char *, TickType_t, BaseType_t,
                                          void *, TimerCallbackFunction_t) {
    return (TimerHandle_t)1;
}
static inline BaseType_t xTimerStart(TimerHandle_t, TickType_t) { return pdPASS; }
static inline BaseType_t xTimerStop(TimerHandle_t, TickType_t) { return pdPASS; }
static inline BaseType_t xTimerDelete(TimerHandle_t, TickType_t) { return pdPASS; }
static inline BaseType_t xTimerReset(TimerHandle_t, TickType_t) { return pdPASS; }
static inline BaseType_t xTimerChangePeriod(TimerHandle_t, TickType_t, TickType_t) { return pdPASS; }

#endif
