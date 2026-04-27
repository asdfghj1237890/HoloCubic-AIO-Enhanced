#ifndef AIO_STUB_ESP_SYSTEM_H
#define AIO_STUB_ESP_SYSTEM_H

// Minimal subset of esp-idf esp_system.h. Avoid pulling in Arduino.h
// here — game_snake_gui.c is a C compilation unit and would choke on
// the C++ STL headers (<string>, <chrono>, <thread>) Arduino.h drags in.

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t esp_random(void) { return (uint32_t)rand(); }
static inline void esp_restart(void) {}

#ifdef __cplusplus
}
#endif

#endif
