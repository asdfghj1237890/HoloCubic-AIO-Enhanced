#ifndef AIO_UNIT_STUB_FREERTOS_TIMERS_H
#define AIO_UNIT_STUB_FREERTOS_TIMERS_H
// Minimal FreeRTOS timer surface — app_controller.h declares
//   TimerHandle_t xTimerEventDeal;
// as a member. The unit test never constructs an AppController, so we
// only need the typedef to exist for the header to compile.
typedef void *TimerHandle_t;
#endif
