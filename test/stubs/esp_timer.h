#ifndef AIO_STUB_ESP_TIMER_H
#define AIO_STUB_ESP_TIMER_H

// Minimal host stub of esp-idf esp_timer.h — just enough for the stockmarket
// app's refresh guard (AIO_Firmware_PIO/src/app/stockmarket/stockmarket.cpp).
// On real firmware the IDF header is used; on the native_test SDL2 host build
// every call is a no-op so the app links. Mirrors the stub pattern used by
// esp_system.h / esp_heap_caps.h in this directory.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK 0
#endif

typedef struct esp_timer *esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void *arg);

typedef enum {
    ESP_TIMER_TASK,
    ESP_TIMER_ISR,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

static inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                                         esp_timer_handle_t *out_handle) {
    (void)args;
    if (out_handle) { *out_handle = (esp_timer_handle_t)1; } // non-null so arm() proceeds
    return ESP_OK;
}
static inline esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us) {
    (void)timer; (void)timeout_us; return ESP_OK;
}
static inline esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    (void)timer; return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif
