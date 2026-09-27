// test/test_regress_light_cfg/test_regress_light_cfg.c
// Regression for review item 9: LIGHTS.CFG values must be range-checked
// before narrowing casts. Out-of-range values clamp (or revert to default
// where a clamp makes no sense).
//
// Contract (TASKS.md, Task 9):
//   r/g/b    clamp to 0..255          scale    clamp to 0..100
//   gamma    non-numeric or <= 0 -> 2.2 default; otherwise clamp to 1.0..3.0
//   gesture  low_pct >= high_pct -> both revert to 20 / 80

#include <unity.h>
#include <stdio.h>
#include "light_cfg.h"

static light_cfg_t g_cfg;

static void parse(const char *globals, const char *seg) {
    static char buf[512];
    snprintf(buf, sizeof(buf),
             "[global]\n%s\n[config]\nname=t\nstart=0\ncount=10\n%s\n", globals, seg);
    TEST_ASSERT_TRUE(light_cfg_parse(buf, &g_cfg));
}

void setUp(void)    {}
void tearDown(void) {}

void test_scale_over_100_clamps(void) {
    parse("", "scale=150");
    TEST_ASSERT_EQUAL_UINT8(100, g_cfg.configs[0].segs[0].scale_pct);
}

void test_color_over_255_clamps(void) {
    parse("", "r=300\ng=256\nb=1000");
    TEST_ASSERT_EQUAL_UINT8(255, g_cfg.configs[0].segs[0].r);
    TEST_ASSERT_EQUAL_UINT8(255, g_cfg.configs[0].segs[0].g);
    TEST_ASSERT_EQUAL_UINT8(255, g_cfg.configs[0].segs[0].b);
}

void test_gamma_zero_uses_default(void) {
    parse("gamma = 0", "");
    TEST_ASSERT_UINT8_WITHIN(2, 56, g_cfg.gamma_lut[128]);   // 2.2 -> ~56
}

void test_gamma_non_numeric_uses_default(void) {
    parse("gamma = abc", "");
    TEST_ASSERT_UINT8_WITHIN(2, 56, g_cfg.gamma_lut[128]);
}

void test_gamma_too_high_clamps_to_3(void) {
    parse("gamma = 5", "");
    TEST_ASSERT_UINT8_WITHIN(2, 32, g_cfg.gamma_lut[128]);   // 3.0 -> ~32
}

void test_gamma_below_1_clamps_to_1(void) {
    parse("gamma = 0.5", "");
    TEST_ASSERT_UINT8_WITHIN(1, 128, g_cfg.gamma_lut[128]);  // 1.0 -> linear
}

void test_gesture_low_not_below_high_reverts_to_defaults(void) {
    parse("gesture_low_pct = 80\ngesture_high_pct = 20", "");
    TEST_ASSERT_EQUAL_UINT8(20, g_cfg.gesture_low_pct);
    TEST_ASSERT_EQUAL_UINT8(80, g_cfg.gesture_high_pct);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scale_over_100_clamps);
    RUN_TEST(test_color_over_255_clamps);
    RUN_TEST(test_gamma_zero_uses_default);
    RUN_TEST(test_gamma_non_numeric_uses_default);
    RUN_TEST(test_gamma_too_high_clamps_to_3);
    RUN_TEST(test_gamma_below_1_clamps_to_1);
    RUN_TEST(test_gesture_low_not_below_high_reverts_to_defaults);
    return UNITY_END();
}
