// Unity tests for analyseParam — the line splitter underlying every
// firmware config parser (SysUtilConfig / SysMpuConfig / RgbConfig and
// every app-side .cfg). It mutates its input buffer in place: each
// '\n' becomes '\0' and argv[i] gets a pointer to the i-th line.
//
// Behavior contract: caller MUST supply at least `argc` newlines; with
// fewer, the inner while-loop walks past the end of the buffer (UB).
// We don't test that case — production callers always pair the parser
// with a write_config that emits exactly the right number of lines.

#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"
#include "Wire.h"
#include "driver/flash_fs.h"

// build_src_filter pulls driver/imu.cpp into every native_unit binary,
// so its globals must resolve at link time. Those (HardwareSerial
// Serial, TwoWire Wire, mpu_fake_*, encoder_*) live in
// test/stubs_unit/test_globals.cpp now — shared across all tests.

void setUp() {}
void tearDown() {}

void test_analyseParam_basic_three_lines() {
    char buf[] = "ssid_value\npassword_value\nNone\n";
    char *param[3] = {0};
    bool ok = analyseParam(buf, 3, param);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("ssid_value", param[0]);
    TEST_ASSERT_EQUAL_STRING("password_value", param[1]);
    TEST_ASSERT_EQUAL_STRING("None", param[2]);
}

void test_analyseParam_mutates_buffer_in_place() {
    char buf[] = "abc\ndef\n";
    char *param[2] = {0};
    analyseParam(buf, 2, param);
    // The '\n' at index 3 must be replaced with '\0'.
    TEST_ASSERT_EQUAL_CHAR('\0', buf[3]);
    // param[0] points to the start, param[1] to the second word.
    TEST_ASSERT_EQUAL_PTR(buf, param[0]);
    TEST_ASSERT_EQUAL_PTR(buf + 4, param[1]);
}

void test_analyseParam_handles_empty_lines() {
    // Empty line in the middle (two newlines in a row) should yield
    // an empty string for that field.
    char buf[] = "first\n\nthird\n";
    char *param[3] = {0};
    analyseParam(buf, 3, param);
    TEST_ASSERT_EQUAL_STRING("first", param[0]);
    TEST_ASSERT_EQUAL_STRING("",      param[1]);
    TEST_ASSERT_EQUAL_STRING("third", param[2]);
}

void test_analyseParam_simulates_sysutil_layout() {
    // Mirrors the 12-field layout AppController::write_config writes
    // for SysUtilConfig (src/sys/app_controller_config.cpp:48-87).
    char buf[] = "ssid0\npass0\nssid1\npass1\nssid2\npass2\n"
                 "1\n80\n4\n1\n0\nNone\n";
    char *param[12] = {0};
    analyseParam(buf, 12, param);
    TEST_ASSERT_EQUAL_STRING("ssid0", param[0]);
    TEST_ASSERT_EQUAL_STRING("pass0", param[1]);
    TEST_ASSERT_EQUAL_STRING("ssid2", param[4]);
    TEST_ASSERT_EQUAL_INT(1,  atol(param[6]));   // power_mode
    TEST_ASSERT_EQUAL_INT(80, atol(param[7]));   // backLight
    TEST_ASSERT_EQUAL_INT(4,  atol(param[8]));   // rotation
    TEST_ASSERT_EQUAL_INT(1,  atol(param[9]));   // auto_calibration_mpu
    TEST_ASSERT_EQUAL_INT(0,  atol(param[10]));  // mpu_order
    TEST_ASSERT_EQUAL_STRING("None", param[11]);
}

void test_analyseParam_handles_partial_argc() {
    // Common pattern: a write produced 12 fields but caller only
    // wants the first N. Should still parse N cleanly.
    char buf[] = "ssid0\npass0\nssid1\npass1\nssid2\npass2\n"
                 "1\n80\n4\n1\n0\nNone\n";
    char *param[3] = {0};
    analyseParam(buf, 3, param);
    TEST_ASSERT_EQUAL_STRING("ssid0", param[0]);
    TEST_ASSERT_EQUAL_STRING("pass0", param[1]);
    TEST_ASSERT_EQUAL_STRING("ssid1", param[2]);
}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_analyseParam_basic_three_lines);
    RUN_TEST(test_analyseParam_mutates_buffer_in_place);
    RUN_TEST(test_analyseParam_handles_empty_lines);
    RUN_TEST(test_analyseParam_simulates_sysutil_layout);
    RUN_TEST(test_analyseParam_handles_partial_argc);
    return UNITY_END();
}
