#ifndef AIO_STUB_ESP_SYSTEM_H
#define AIO_STUB_ESP_SYSTEM_H
#include "Arduino.h"

// Minimal subset of esp-idf esp_system.h used by firmware sources.
// game_snake_gui.c includes the header but doesn't actually call any
// of these in the LVGL paths we exercise; keep them no-op for now.

static inline uint32_t esp_random() { return (uint32_t)rand(); }
static inline void esp_restart() {}

#endif
