#ifndef AIO_STUB_ESP_LOG_H
#define AIO_STUB_ESP_LOG_H

// Minimal esp-idf esp_log.h shim. The macros are no-ops on the host —
// firmware code keeps compiling, but we don't try to mirror logging.

#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)

typedef int esp_log_level_t;
static inline void esp_log_level_set(const char *, esp_log_level_t) {}

#endif
