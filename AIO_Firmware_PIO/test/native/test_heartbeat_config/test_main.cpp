// Unity tests for heartbeat_parse_config — exercises the snprintf-bounded
// field copies in HeartbeatRawFields against oversize / minimum-length /
// empty inputs. Pinned to the bug class fixed in PR-1.2 (Phase 1B):
// the original heartbeat::read_config strncpy'd unbounded fields from a
// possibly-malformed flash config straight into char[32] / char[16] /
// char[20] struct members.

#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"  // host stub satisfies analyse_param.cpp's includes
#include "Wire.h"
#include "driver/flash_fs.h"
#include "app/heartbeat/heartbeat_config_parse.h"

void setUp() {}
void tearDown() {}

// Helper: build a six-line cfg buffer using individual field strings.
// Mutable buffer because heartbeat_parse_config + analyseParam mutate
// in place. Caller must size the storage; we pass a generous 256 byte
// scratch.
static void make_cfg(char *dst, size_t cap,
                     const char *server, const char *port,
                     const char *user, const char *pass,
                     const char *role, const char *qq) {
    snprintf(dst, cap, "%s\n%s\n%s\n%s\n%s\n%s\n",
             server, port, user, pass, role, qq);
}

void test_well_formed_config_parses_all_six_fields() {
    char buf[256];
    make_cfg(buf, sizeof(buf),
             "broker.example.com", "1883",
             "alice", "s3cret",
             "0", "12345678");
    HeartbeatRawFields out{};
    bool ok = heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("broker.example.com", out.mqtt_server);
    TEST_ASSERT_EQUAL_UINT16(1883, out.mqtt_port);
    TEST_ASSERT_EQUAL_STRING("alice", out.mqtt_user);
    TEST_ASSERT_EQUAL_STRING("s3cret", out.mqtt_password);
    TEST_ASSERT_EQUAL_INT(0, out.role);
    TEST_ASSERT_EQUAL_STRING("12345678", out.qq_num);
}

void test_empty_buffer_returns_false() {
    HeartbeatRawFields out{};
    char buf[1] = {0};
    bool ok = heartbeat_parse_config(buf, 0, &out);
    TEST_ASSERT_FALSE(ok);
}

void test_null_out_returns_false() {
    char buf[16] = "x\n80\nu\np\n0\nq\n";
    bool ok = heartbeat_parse_config(buf, strlen(buf), nullptr);
    TEST_ASSERT_FALSE(ok);
}

void test_oversize_mqtt_server_truncates_to_31_chars() {
    // The struct's mqtt_server is char[32]; snprintf must keep at most 31
    // chars + a terminating NUL. The original strncpy left no NUL when
    // src length >= dst size, which downstream code then read past.
    char buf[256];
    make_cfg(buf, sizeof(buf),
             "this-server-name-is-longer-than-thirty-one-characters",
             "1883", "u", "p", "0", "q");
    HeartbeatRawFields out{};
    heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_EQUAL_size_t(31, strlen(out.mqtt_server));
    TEST_ASSERT_EQUAL_CHAR('\0', out.mqtt_server[31]);
    TEST_ASSERT_EQUAL_STRING_LEN("this-server-name-is-longer-than", out.mqtt_server, 31);
}

void test_oversize_mqtt_user_truncates_to_15_chars() {
    char buf[256];
    make_cfg(buf, sizeof(buf),
             "host", "1883",
             "very-long-username-overflows-cap",
             "p", "0", "q");
    HeartbeatRawFields out{};
    heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_EQUAL_size_t(15, strlen(out.mqtt_user));
    TEST_ASSERT_EQUAL_CHAR('\0', out.mqtt_user[15]);
    TEST_ASSERT_EQUAL_STRING_LEN("very-long-usern", out.mqtt_user, 15);
}

void test_oversize_mqtt_password_truncates_to_15_chars() {
    char buf[256];
    make_cfg(buf, sizeof(buf),
             "host", "1883", "u",
             "this-password-is-way-too-long-for-the-cap",
             "0", "q");
    HeartbeatRawFields out{};
    heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_EQUAL_size_t(15, strlen(out.mqtt_password));
    TEST_ASSERT_EQUAL_CHAR('\0', out.mqtt_password[15]);
}

void test_oversize_qq_num_truncates_to_19_chars() {
    char buf[256];
    make_cfg(buf, sizeof(buf),
             "host", "1883", "u", "p", "0",
             "user-id-string-is-too-long-for-twenty-byte-buffer");
    HeartbeatRawFields out{};
    heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_EQUAL_size_t(19, strlen(out.qq_num));
    TEST_ASSERT_EQUAL_CHAR('\0', out.qq_num[19]);
}

void test_empty_string_fields_yield_empty_strings() {
    // A config with empty server / user / password (web-setting reset)
    // must produce zero-length strings, not garbage from prior memory.
    char buf[256];
    make_cfg(buf, sizeof(buf), "", "0", "", "", "0", "");
    HeartbeatRawFields out{};
    out.mqtt_server[0] = 'X';   // poison to detect failure-to-write
    out.mqtt_user[0] = 'X';
    out.mqtt_password[0] = 'X';
    out.qq_num[0] = 'X';
    bool ok = heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", out.mqtt_server);
    TEST_ASSERT_EQUAL_STRING("", out.mqtt_user);
    TEST_ASSERT_EQUAL_STRING("", out.mqtt_password);
    TEST_ASSERT_EQUAL_STRING("", out.qq_num);
}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_well_formed_config_parses_all_six_fields);
    RUN_TEST(test_empty_buffer_returns_false);
    RUN_TEST(test_null_out_returns_false);
    RUN_TEST(test_oversize_mqtt_server_truncates_to_31_chars);
    RUN_TEST(test_oversize_mqtt_user_truncates_to_15_chars);
    RUN_TEST(test_oversize_mqtt_password_truncates_to_15_chars);
    RUN_TEST(test_oversize_qq_num_truncates_to_19_chars);
    RUN_TEST(test_empty_string_fields_yield_empty_strings);
    return UNITY_END();
}
