#ifndef AIO_UNIT_STUB_LVGL_H
#define AIO_UNIT_STUB_LVGL_H

// Minimal lvgl.h shim. The real header chain is huge; for unit-testing
// IMU::getAction we only need the two symbols that src/driver/
// lv_port_indev.h drags in transitively from src/driver/imu.h.
//
// Same-directory include search means src/driver/lv_port_indev.h wins
// over our test/stubs_unit/lv_port_indev.h, so we provide what IT needs
// instead of trying to shadow it.

typedef enum {
    LV_INDEV_STATE_RELEASED = 0,
    LV_INDEV_STATE_PRESSED  = 1
} lv_indev_state_t;

typedef struct _lv_indev_t lv_indev_t;

#endif
