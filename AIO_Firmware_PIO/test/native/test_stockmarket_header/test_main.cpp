#include <unity.h>
#include <string.h>

#include "app/stockmarket/stockmarket_header.h"

void setUp() {}
void tearDown() {}

static void format_header(char *out,
                          size_t out_len,
                          const char *symbol,
                          const char *company)
{
    memset(out, 0xA5, out_len);
    stockmarket_format_header(out, out_len, symbol, company);
}

static bool is_valid_utf8(const char *text)
{
    for (size_t i = 0; text[i] != '\0';)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        size_t len = 1;
        if (c < 0x80)
        {
            ++i;
            continue;
        }
        if ((c & 0xE0) == 0xC0)
        {
            len = 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            len = 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            len = 4;
        }
        else
        {
            return false;
        }

        for (size_t j = 1; j < len; ++j)
        {
            const unsigned char next = static_cast<unsigned char>(text[i + j]);
            if (next == '\0' || (next & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += len;
    }

    return true;
}

void test_empty_symbol_uses_placeholder()
{
    char out[32];
    format_header(out, sizeof(out), "", "Apple Inc.");
    TEST_ASSERT_EQUAL_STRING("--", out);
}

void test_symbol_without_company_is_kept()
{
    char out[32];
    format_header(out, sizeof(out), "AAPL", "");
    TEST_ASSERT_EQUAL_STRING("AAPL", out);
}

void test_normal_symbol_and_company_are_kept()
{
    char out[32];
    format_header(out, sizeof(out), "AAPL", "Apple Inc.");
    TEST_ASSERT_EQUAL_STRING("AAPL - Apple Inc.", out);
}

void test_long_company_name_is_shortened_to_one_header_line()
{
    char out[48];
    format_header(out, sizeof(out), "LONG", "Very Long Company Name");

    TEST_ASSERT_LESS_OR_EQUAL(STOCKMARKET_HEADER_MAX_COLUMNS,
                              stockmarket_header_display_columns(out));
    TEST_ASSERT_TRUE_MESSAGE(strncmp(out, "LONG - ", 7) == 0, out);
    TEST_ASSERT_NOT_NULL(strstr(out, "..."));
    TEST_ASSERT_NULL(strstr(out, "Company Name"));
}

void test_utf8_company_name_is_not_split_mid_character()
{
    char out[48];
    format_header(out, sizeof(out), "2330", "台灣積體電路製造股份有限公司");

    TEST_ASSERT_LESS_OR_EQUAL(STOCKMARKET_HEADER_MAX_COLUMNS,
                              stockmarket_header_display_columns(out));
    TEST_ASSERT_NOT_NULL(strstr(out, "..."));
    TEST_ASSERT_TRUE_MESSAGE(is_valid_utf8(out), out);
}

void test_small_buffer_does_not_split_utf8()
{
    char out[18];
    format_header(out, sizeof(out), "2330", "台灣積體電路製造股份有限公司");

    TEST_ASSERT_TRUE_MESSAGE(is_valid_utf8(out), out);
    TEST_ASSERT_NOT_NULL(strstr(out, "..."));
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_symbol_uses_placeholder);
    RUN_TEST(test_symbol_without_company_is_kept);
    RUN_TEST(test_normal_symbol_and_company_are_kept);
    RUN_TEST(test_long_company_name_is_shortened_to_one_header_line);
    RUN_TEST(test_utf8_company_name_is_not_split_mid_character);
    RUN_TEST(test_small_buffer_does_not_split_utf8);
    return UNITY_END();
}
