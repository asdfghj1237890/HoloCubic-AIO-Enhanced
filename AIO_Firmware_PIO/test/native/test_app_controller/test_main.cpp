// Unity tests for send_to_dispatch — the queue/dispatch logic
// extracted from AppController::send_to (src/sys/app_controller.cpp).
// We test the free function directly so the suite doesn't have to
// stand up the full controller (FreeRTOS timers + WiFi globals etc).

#include <unity.h>
#include <list>
#include <string.h>
#include "Arduino.h"
#include "Wire.h"
#include "sys/send_to_dispatch.h"
#include "sys/app_controller.h"  // EVENT_OBJ + EVENT_LIST_MAX_LENGTH

// Counter that fake message_handle bumps so tests can assert dispatch.
static int g_handler_call_count = 0;
static APP_MESSAGE_TYPE g_handler_last_type = APP_MESSAGE_NONE;
static const char *g_handler_last_from = nullptr;

static void fake_message_handle(const char *from, const char * /*to*/,
                                APP_MESSAGE_TYPE type, void * /*message*/,
                                void * /*ext_info*/) {
    ++g_handler_call_count;
    g_handler_last_type = type;
    g_handler_last_from = from;
}

void setUp() {
    g_handler_call_count = 0;
    g_handler_last_type = APP_MESSAGE_NONE;
    g_handler_last_from = nullptr;
}

void tearDown() {}

static APP_OBJ make_app(const char *name,
                        void (*mh)(const char *, const char *,
                                   APP_MESSAGE_TYPE, void *, void *) = nullptr) {
    APP_OBJ a = {};
    a.app_name = name;
    a.message_handle = mh;
    return a;
}

// --- queue path: type <= APP_MESSAGE_MQTT_DATA ---

void test_queue_path_pushes_event_and_returns_zero() {
    std::list<EVENT_OBJ> ev;
    APP_OBJ from_app = make_app("sender");
    int rc = send_to_dispatch(&from_app, nullptr, "sender", "AppCtrl",
                              APP_MESSAGE_WIFI_CONN, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, ev.size());
    TEST_ASSERT_EQUAL(APP_MESSAGE_WIFI_CONN, ev.front().type);
    TEST_ASSERT_EQUAL_PTR(&from_app, ev.front().from);
    TEST_ASSERT_EQUAL(5, ev.front().retryMaxNum);
}

void test_queue_path_returns_one_when_full() {
    // The cap is `eventList.size() > EVENT_LIST_MAX_LENGTH`, so the
    // 11th item still fits; the 12th call should be rejected.
    std::list<EVENT_OBJ> ev;
    APP_OBJ from_app = make_app("sender");
    for (int i = 0; i < EVENT_LIST_MAX_LENGTH + 1; ++i) {
        int rc = send_to_dispatch(&from_app, nullptr, "sender", "AppCtrl",
                                  APP_MESSAGE_WIFI_ALIVE, nullptr, nullptr, &ev);
        TEST_ASSERT_EQUAL(0, rc);
    }
    int rc = send_to_dispatch(&from_app, nullptr, "sender", "AppCtrl",
                              APP_MESSAGE_WIFI_ALIVE, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(1, rc);
    TEST_ASSERT_EQUAL_size_t(EVENT_LIST_MAX_LENGTH + 1, ev.size());
}

void test_queue_path_at_boundary_type_mqtt_data() {
    // APP_MESSAGE_MQTT_DATA is on the queue side of the boundary
    // (the firmware uses `<=`). Just above (GET_PARAM) is dispatched.
    std::list<EVENT_OBJ> ev;
    APP_OBJ from_app = make_app("sender");
    int rc = send_to_dispatch(&from_app, nullptr, "sender", "AppCtrl",
                              APP_MESSAGE_MQTT_DATA, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, ev.size());
}

// --- dispatch path: type > APP_MESSAGE_MQTT_DATA ---

void test_dispatch_path_invokes_message_handle() {
    std::list<EVENT_OBJ> ev;
    APP_OBJ from_app = make_app("sender");
    APP_OBJ to_app   = make_app("receiver", fake_message_handle);
    int rc = send_to_dispatch(&from_app, &to_app, "sender", "receiver",
                              APP_MESSAGE_GET_PARAM, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL(1, g_handler_call_count);
    TEST_ASSERT_EQUAL(APP_MESSAGE_GET_PARAM, g_handler_last_type);
    TEST_ASSERT_EQUAL_STRING("sender", g_handler_last_from);
    // Dispatch path doesn't touch the queue.
    TEST_ASSERT_EQUAL_size_t(0, ev.size());
}

void test_dispatch_path_skips_null_handler_without_crashing() {
    std::list<EVENT_OBJ> ev;
    APP_OBJ to_app = make_app("receiver", nullptr);  // no handler
    int rc = send_to_dispatch(nullptr, &to_app, "sender", "receiver",
                              APP_MESSAGE_GET_PARAM, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL(0, g_handler_call_count);
}

void test_dispatch_path_returns_two_when_to_app_missing() {
    // Caller (AppController::send_to) maps rc=2 to: "if `to` matches
    // CTRL_NAME, route through deal_config; otherwise treat as 0/no-op".
    std::list<EVENT_OBJ> ev;
    int rc = send_to_dispatch(nullptr, nullptr, "sender", "AppCtrl",
                              APP_MESSAGE_SET_PARAM, nullptr, nullptr, &ev);
    TEST_ASSERT_EQUAL(2, rc);
    TEST_ASSERT_EQUAL(0, g_handler_call_count);
    TEST_ASSERT_EQUAL_size_t(0, ev.size());
}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_queue_path_pushes_event_and_returns_zero);
    RUN_TEST(test_queue_path_returns_one_when_full);
    RUN_TEST(test_queue_path_at_boundary_type_mqtt_data);
    RUN_TEST(test_dispatch_path_invokes_message_handle);
    RUN_TEST(test_dispatch_path_skips_null_handler_without_crashing);
    RUN_TEST(test_dispatch_path_returns_two_when_to_app_missing);
    return UNITY_END();
}
