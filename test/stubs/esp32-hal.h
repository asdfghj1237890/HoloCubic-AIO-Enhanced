#ifndef AIO_STUB_ESP32_HAL_H
#define AIO_STUB_ESP32_HAL_H
// ESP32 generic hal — the firmware code only uses the small slice already
// stubbed elsewhere (millis, delay, etc.). This wrapper just routes back
// to Arduino.h.
#include "Arduino.h"
#endif
