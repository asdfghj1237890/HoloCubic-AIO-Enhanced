#ifndef AIO_STUB_ESP_HEAP_CAPS_H
#define AIO_STUB_ESP_HEAP_CAPS_H

// Minimal esp_heap_caps shim. PSRAM/internal alloc collapses to malloc.

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MALLOC_CAP_SPIRAM   0x00000001
#define MALLOC_CAP_INTERNAL 0x00000002
#define MALLOC_CAP_DMA      0x00000004
#define MALLOC_CAP_8BIT     0x00000008
#define MALLOC_CAP_DEFAULT  0x00000010
#define MALLOC_CAP_32BIT    0x00000020

static inline void *heap_caps_malloc(size_t s, uint32_t) { return malloc(s); }
static inline void *heap_caps_calloc(size_t n, size_t s, uint32_t) { return calloc(n, s); }
static inline void heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_free_size(uint32_t) { return 0; }
static inline size_t heap_caps_get_largest_free_block(uint32_t) { return 0; }

#ifdef __cplusplus
}
#endif

#endif
