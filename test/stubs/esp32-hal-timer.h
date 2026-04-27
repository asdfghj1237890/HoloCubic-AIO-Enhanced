#ifndef AIO_STUB_ESP32_HAL_TIMER_H
#define AIO_STUB_ESP32_HAL_TIMER_H

// Minimal subset of the ESP32 hal-timer API. Pulled in indirectly by
// bilibili_fans/bilibili.cpp -> "../../common.h" -> firmware
// driver/rgb_led.h. The harness never starts a real timer; just keep
// the type opaque enough to compile.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hw_timer_s hw_timer_t;

static inline hw_timer_t *timerBegin(uint8_t, uint16_t, bool) { return (hw_timer_t *)0; }
static inline void timerEnd(hw_timer_t *) {}
static inline void timerAttachInterrupt(hw_timer_t *, void (*)(void), bool) {}
static inline void timerDetachInterrupt(hw_timer_t *) {}
static inline void timerAlarmEnable(hw_timer_t *) {}
static inline void timerAlarmDisable(hw_timer_t *) {}
static inline void timerAlarmWrite(hw_timer_t *, uint64_t, bool) {}
static inline void timerStart(hw_timer_t *) {}
static inline void timerStop(hw_timer_t *) {}
static inline void timerWrite(hw_timer_t *, uint64_t) {}
static inline uint64_t timerRead(hw_timer_t *) { return 0; }

#ifdef __cplusplus
}
#endif

#endif
