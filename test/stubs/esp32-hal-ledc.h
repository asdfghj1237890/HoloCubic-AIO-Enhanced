#ifndef AIO_STUB_ESP32_HAL_LEDC_H
#define AIO_STUB_ESP32_HAL_LEDC_H
#include "Arduino.h"
static inline double ledcSetup(uint8_t, double, uint8_t) { return 5000.0; }
static inline void ledcAttachPin(uint8_t, uint8_t) {}
static inline void ledcWrite(uint8_t, uint32_t) {}
static inline uint32_t ledcRead(uint8_t) { return 0; }
#endif
