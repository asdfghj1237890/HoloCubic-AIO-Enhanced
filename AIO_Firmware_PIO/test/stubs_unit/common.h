#ifndef AIO_UNIT_STUB_COMMON_H
#define AIO_UNIT_STUB_COMMON_H
#include "Arduino.h"
#include "Wire.h"  // imu.cpp init() uses Wire.begin / setClock; the real
                   // common.h pulls Wire transitively, so match that.
#include "freertos/timers.h"  // TimerHandle_t — app_controller.h has one
                              // as a member.

// Pin constants imu.cpp references during init() (which the unit test
// never invokes — but they need to resolve at compile time).
#define IMU_I2C_SDA 32
#define IMU_I2C_SCL 33

// imu.cpp's init() loop uses these. The body only runs if init() runs;
// our tests skip init() and call getAction directly, so dummies are
// fine.
#define GET_SYS_MILLIS() (0UL)
inline boolean doDelayMillisTime(unsigned long, unsigned long *, boolean state) {
    return state;
}

#endif
