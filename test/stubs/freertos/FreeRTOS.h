#ifndef AIO_STUB_FREERTOS_H
#define AIO_STUB_FREERTOS_H
#include "Arduino.h"

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void *TaskHandle_t;
typedef void *QueueHandle_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define portTICK_PERIOD_MS 1u
#define configMAX_PRIORITIES 25
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

static inline TickType_t xTaskGetTickCount() { return millis(); }
static inline void vTaskDelay(uint32_t ticks) { delay(ticks); }
static inline void vTaskDelete(TaskHandle_t) {}

static inline BaseType_t xTaskCreatePinnedToCore(
    void (*)(void *), const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *, BaseType_t) {
    return pdPASS;
}
static inline BaseType_t xTaskCreate(
    void (*)(void *), const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *) {
    return pdPASS;
}
static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }

#endif
