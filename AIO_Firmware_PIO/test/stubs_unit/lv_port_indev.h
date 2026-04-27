#ifndef AIO_UNIT_STUB_LV_PORT_INDEV_H
#define AIO_UNIT_STUB_LV_PORT_INDEV_H

// imu.h includes lv_port_indev.h purely for the lv_indev_state_t enum
// (used by the encoder_state extern). For Track B we don't want to
// drag all of LVGL into a unit-test binary, so we provide the smallest
// definition that lets imu.h compile.

typedef enum {
    LV_INDEV_STATE_RELEASED = 0,
    LV_INDEV_STATE_PRESSED  = 1
} lv_indev_state_t;

#endif
