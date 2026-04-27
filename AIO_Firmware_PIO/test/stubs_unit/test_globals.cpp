// Globals that every native_unit test binary needs to resolve at link
// time — driver/imu.cpp gets compiled into every test (via the env's
// build_src_filter), so its references to Wire / Serial / mpu_fake_*
// must be defined somewhere even in tests that don't touch IMU.
//
// This file is added to build_src_filter as
//   +<../test/stubs_unit/test_globals.cpp>
// so it lives alongside each test binary's own test_main.cpp.
//
// Tests that DO want to drive the IMU (see test_imu_action) read/write
// the mpu_fake_* values directly — they're plain globals, not const.

#include "Arduino.h"
#include "Wire.h"
#include "MPU6050.h"
#include "lvgl.h"

HardwareSerial Serial;
TwoWire Wire;

int16_t mpu_fake_ax = 0, mpu_fake_ay = 0, mpu_fake_az = 0;
int16_t mpu_fake_gx = 0, mpu_fake_gy = 0, mpu_fake_gz = 0;

// imu.h declares these as extern (the firmware's lv_port_indev module
// owns them on-device). Define them here so any test that pulls in
// imu.cpp links cleanly.
int32_t encoder_diff = 0;
lv_indev_state_t encoder_state = LV_INDEV_STATE_RELEASED;
