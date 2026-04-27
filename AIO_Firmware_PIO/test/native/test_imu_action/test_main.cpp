// Unity unit tests for IMU::getAction (the gesture-classification logic
// in src/driver/imu.cpp). The MPU6050 stub in test/stubs_unit/ exposes
// mpu_set_motion(...) so each test can inject canned accelerometer
// values, then assert which ACTIVE_TYPE getAction infers.
//
// We skip IMU::init() entirely — order=0 from the constructor is what
// production also uses by default, and init() only touches the I2C
// bus / calibration paths we don't care about here.

#include <unity.h>
#include "Arduino.h"
#include "Wire.h"
#include "driver/imu.h"

// HardwareSerial Serial / TwoWire Wire / mpu_fake_* / encoder_* are all
// defined once in test/stubs_unit/test_globals.cpp (compiled into every
// test binary).

static IMU *imu;

void setUp() {
    imu = new IMU();
    mpu_set_motion(0, 0, 16384);  // gravity-only neutral pose
}

void tearDown() {
    delete imu;
    imu = nullptr;
}

// Helper: clear the per-IMU action_info between calls so a previous
// test's "isValid=1" doesn't suppress short-press detection.
static void reset_action_info() {
    imu->action_info.isValid = 0;
    imu->action_info.active = ACTIVE_TYPE::UNKNOWN;
}

// --- v_ay axis: TURN_LEFT / TURN_RIGHT thresholds ---

void test_turn_left_when_v_ay_above_4000() {
    mpu_set_motion(0, 5000, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::TURN_LEFT, a->active);
}

void test_turn_right_when_v_ay_below_neg_4000() {
    mpu_set_motion(0, -5000, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::TURN_RIGHT, a->active);
}

void test_shake_when_v_ay_in_jitter_band() {
    // Just above the 1000 jitter floor but below the 4000 turn threshold.
    mpu_set_motion(0, 2000, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::SHAKE, a->active);
}

// --- v_ax axis: UP / DOWN thresholds ---

void test_up_when_v_ax_above_5000() {
    mpu_set_motion(6000, 0, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::UP, a->active);
}

void test_down_when_v_ax_below_neg_5000() {
    mpu_set_motion(-6000, 0, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::DOWN, a->active);
}

// --- below thresholds ---

void test_unknown_when_below_all_thresholds() {
    mpu_set_motion(500, 500, 16384);
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::UNKNOWN, a->active);
}

// --- long-press promotion: 3 consecutive samples upgrade UP/DOWN ---

void test_three_consecutive_ups_promote_to_go_forword() {
    mpu_set_motion(6000, 0, 16384);
    // First two samples register isValid=1 with UP, but reset between
    // calls so the short-press path keeps firing. The third call still
    // sees the same act_info_history[index/-1/-2] == UP triple.
    reset_action_info(); imu->getAction();
    reset_action_info(); imu->getAction();
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::GO_FORWORD, a->active);
}

void test_three_consecutive_downs_promote_to_return() {
    mpu_set_motion(-6000, 0, 16384);
    reset_action_info(); imu->getAction();
    reset_action_info(); imu->getAction();
    reset_action_info();
    ImuAction *a = imu->getAction();
    TEST_ASSERT_EQUAL(ACTIVE_TYPE::RETURN, a->active);
}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_turn_left_when_v_ay_above_4000);
    RUN_TEST(test_turn_right_when_v_ay_below_neg_4000);
    RUN_TEST(test_shake_when_v_ay_in_jitter_band);
    RUN_TEST(test_up_when_v_ax_above_5000);
    RUN_TEST(test_down_when_v_ax_below_neg_5000);
    RUN_TEST(test_unknown_when_below_all_thresholds);
    RUN_TEST(test_three_consecutive_ups_promote_to_go_forword);
    RUN_TEST(test_three_consecutive_downs_promote_to_return);
    return UNITY_END();
}
