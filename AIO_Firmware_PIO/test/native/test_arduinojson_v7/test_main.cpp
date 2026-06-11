#include <unity.h>
#include <ArduinoJson.h>
#include <string>

void setUp() {}
void tearDown() {}

void test_arduinojson_v7_header_is_selected()
{
    TEST_ASSERT_EQUAL(7, ARDUINOJSON_VERSION_MAJOR);
}

void test_jsondocument_supports_current_firmware_idioms()
{
    JsonDocument doc;
    doc["meta"]["regularMarketPrice"] = 123.45;
    doc["meta"]["symbol"] = "AAPL";

    TEST_ASSERT_TRUE(doc["meta"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["meta"]["regularMarketPrice"].is<float>());
    TEST_ASSERT_EQUAL_FLOAT(123.45f, doc["meta"]["regularMarketPrice"] | 0.0f);
    TEST_ASSERT_EQUAL_STRING("AAPL", doc["meta"]["symbol"] | "");
}

void test_serialize_json_replaces_string_content()
{
    JsonDocument doc;
    doc["ok"] = true;

    std::string out = "stale";
    serializeJson(doc, out);

    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", out.c_str());
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_arduinojson_v7_header_is_selected);
    RUN_TEST(test_jsondocument_supports_current_firmware_idioms);
    RUN_TEST(test_serialize_json_replaces_string_content);
    return UNITY_END();
}
