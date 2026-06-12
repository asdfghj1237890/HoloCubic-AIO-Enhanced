#include <unity.h>
#include <string.h>

#include "app/server/web_responsive_css.h"

void setUp() {}
void tearDown() {}

static void assert_contains(const char *haystack, const char *needle)
{
    TEST_ASSERT_NOT_NULL(haystack);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(haystack, needle), needle);
}

void test_glass_css_has_mobile_layout_breakpoint()
{
    const char *css = web_responsive_css();

    assert_contains(css, "@media (max-width: 860px)");
    assert_contains(css, ".app{display:flex;flex-direction:column");
    assert_contains(css, ".nav-group{flex:0 0 auto;display:flex;overflow-x:auto");
    assert_contains(css, ".field{grid-template-columns:1fr");
    assert_contains(css, ".hero .kpi-grid{grid-template-columns:repeat(2,minmax(0,1fr))");
}

void test_glass_css_has_narrow_phone_overrides()
{
    const char *css = web_responsive_css();

    assert_contains(css, "@media (max-width: 420px)");
    assert_contains(css, ".hero .kpi-grid{grid-template-columns:1fr");
    assert_contains(css, ".content{padding:14px 10px 24px");
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_glass_css_has_mobile_layout_breakpoint);
    RUN_TEST(test_glass_css_has_narrow_phone_overrides);
    return UNITY_END();
}
