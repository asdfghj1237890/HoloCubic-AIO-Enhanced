#ifndef AIO_STUB_FREERTOS_QUEUE_H
#define AIO_STUB_FREERTOS_QUEUE_H
#include "FreeRTOS.h"

static inline QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t) { return (QueueHandle_t)1; }
static inline BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t) { return pdPASS; }
static inline BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t) { return pdFAIL; }
static inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t) { return 0; }
static inline void vQueueDelete(QueueHandle_t) {}

#endif
