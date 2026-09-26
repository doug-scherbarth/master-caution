// test/test_light_cfg/test_light_cfg.c
// Unity tests for the light_cfg module.
// Run: pio test -e host -f test_light_cfg

#include <unity.h>
#include "light_cfg.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

void hal_mock_reset(void);
void hal_mock_set_sd_file(const char *content);

void setUp(void)    { hal_mock_reset(); }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char MINIMAL_CFG[] =
    "[global]\n"
    "total_pixels = 10\n"
    "mA_per_channel = 15\n"
    "quiescent_mA = 2\n"
    "max_current_mA = 1000\n"
    "gamma = 1.0\n"
    "[config]\n"
    "name = test\n"
    "start = 0\n"
    "count = 10\n"
    "r = 200\n"
    "g = 100\n"
    "b = 50\n"
    "scale = 80\n";

static const char TWO_SEG_CFG[] =
    "[global]\n"
    "total_pixels = 20\n"
    "[config]\n"
    "name = twoseg\n"
    "start = 0\n"
    "count = 10\n"
    "r = 255\n"
    "g = 0\n"
    "b = 0\n"
    "scale = 100\n"
    "start = 10\n"
    "count = 10\n"
    "r = 0\n"
    "g = 0\n"
    "b = 255\n"
    "scale = 50\n";

static const char TWO_CFG[] =
    "[global]\n"
    "total_pixels = 8\n"
    "[config]\n"
    "name = bright\n"
    "start = 0\n"
    "count = 8\n"
    "r = 255\n"
    "g = 255\n"
    "b = 255\n"
    "scale = 100\n"
    "[config]\n"
    "name = dim\n"
    "start = 0\n"
    "count = 8\n"
    "r = 255\n"
    "g = 255\n"
    "b = 255\n"
    "scale = 10\n";

// ---------------------------------------------------------------------------
// Parse: basic fields
// ---------------------------------------------------------------------------

void test_parse_global_fields(void) {
    light_cfg_t cfg;
    bool ok = light_cfg_parse(MINIMAL_CFG, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT16(10,   cfg.total_pixels);
    TEST_ASSERT_EQUAL_UINT16(15,   cfg.mA_per_channel);
    TEST_ASSERT_EQUAL_UINT16(2,    cfg.quiescent_mA);
    TEST_ASSERT_EQUAL_UINT16(1000, cfg.max_current_mA);
}

void test_parse_config_name_and_segment(void) {
    light_cfg_t cfg;
    bool ok = light_cfg_parse(MINIMAL_CFG, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("test", cfg.configs[0].name);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.configs[0].n_segs);
    TEST_ASSERT_EQUAL_UINT16(0,  cfg.configs[0].segs[0].start);
    TEST_ASSERT_EQUAL_UINT16(10, cfg.configs[0].segs[0].count);
    TEST_ASSERT_EQUAL_UINT8(200, cfg.configs[0].segs[0].r);
    TEST_ASSERT_EQUAL_UINT8(100, cfg.configs[0].segs[0].g);
    TEST_ASSERT_EQUAL_UINT8(50,  cfg.configs[0].segs[0].b);
    TEST_ASSERT_EQUAL_UINT8(80,  cfg.configs[0].segs[0].scale_pct);
}

// ---------------------------------------------------------------------------
// Parse: multiple segments within one [config]
// ---------------------------------------------------------------------------

void test_parse_two_segments(void) {
    light_cfg_t cfg;
    bool ok = light_cfg_parse(TWO_SEG_CFG, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_UINT8(2, cfg.configs[0].n_segs);

    TEST_ASSERT_EQUAL_UINT16(0,  cfg.configs[0].segs[0].start);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.configs[0].segs[0].r);
    TEST_ASSERT_EQUAL_UINT8(0,   cfg.configs[0].segs[0].b);
    TEST_ASSERT_EQUAL_UINT8(100, cfg.configs[0].segs[0].scale_pct);

    TEST_ASSERT_EQUAL_UINT16(10, cfg.configs[0].segs[1].start);
    TEST_ASSERT_EQUAL_UINT8(0,   cfg.configs[0].segs[1].r);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.configs[0].segs[1].b);
    TEST_ASSERT_EQUAL_UINT8(50,  cfg.configs[0].segs[1].scale_pct);
}

// ---------------------------------------------------------------------------
// Parse: multiple [config] sections
// ---------------------------------------------------------------------------

void test_parse_two_configs(void) {
    light_cfg_t cfg;
    bool ok = light_cfg_parse(TWO_CFG, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(2, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("bright", cfg.configs[0].name);
    TEST_ASSERT_EQUAL_STRING("dim",    cfg.configs[1].name);
    TEST_ASSERT_EQUAL_UINT8(100, cfg.configs[0].segs[0].scale_pct);
    TEST_ASSERT_EQUAL_UINT8(10,  cfg.configs[1].segs[0].scale_pct);
}

// ---------------------------------------------------------------------------
// Parse: returns false when no [config] sections
// ---------------------------------------------------------------------------

void test_parse_no_config_returns_false(void) {
    const char *global_only = "[global]\ntotal_pixels = 10\n";
    light_cfg_t cfg;
    bool ok = light_cfg_parse(global_only, &cfg);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT8(0, cfg.n_configs);
}

void test_parse_empty_returns_false(void) {
    light_cfg_t cfg;
    bool ok = light_cfg_parse("", &cfg);
    TEST_ASSERT_FALSE(ok);
}

// ---------------------------------------------------------------------------
// Parse: total_pixels clamped to LIGHT_MAX_PIXELS
// ---------------------------------------------------------------------------

void test_parse_total_pixels_clamped(void) {
    const char *big =
        "[global]\ntotal_pixels = 9999\n"
        "[config]\nname=x\nstart=0\ncount=1\nr=0\ng=0\nb=0\nscale=100\n";
    light_cfg_t cfg;
    light_cfg_parse(big, &cfg);
    TEST_ASSERT_EQUAL_UINT16(LIGHT_MAX_PIXELS, cfg.total_pixels);
}

// ---------------------------------------------------------------------------
// Parse: overflow configs silently dropped
// ---------------------------------------------------------------------------

void test_parse_overflow_configs_capped(void) {
    // build a string with LIGHT_MAX_CONFIGS+2 [config] sections
    char buf[2048] = "[global]\ntotal_pixels=4\n";
    for (int i = 0; i < (int)LIGHT_MAX_CONFIGS + 2; i++) {
        char seg[128];
        snprintf(seg, sizeof(seg),
                 "[config]\nname=c%d\nstart=0\ncount=1\nr=1\ng=1\nb=1\nscale=100\n", i);
        strncat(buf, seg, sizeof(buf) - strlen(buf) - 1);
    }
    light_cfg_t cfg;
    bool ok = light_cfg_parse(buf, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(LIGHT_MAX_CONFIGS, cfg.n_configs);
}

// ---------------------------------------------------------------------------
// Parse: overflow segments per config capped
// ---------------------------------------------------------------------------

void test_parse_overflow_segs_capped(void) {
    char buf[2048] = "[global]\ntotal_pixels=50\n[config]\nname=many\n";
    for (int i = 0; i < (int)LIGHT_MAX_SEGS + 2; i++) {
        char seg[128];
        snprintf(seg, sizeof(seg),
                 "start=%d\ncount=1\nr=1\ng=1\nb=1\nscale=100\n", i);
        strncat(buf, seg, sizeof(buf) - strlen(buf) - 1);
    }
    light_cfg_t cfg;
    bool ok = light_cfg_parse(buf, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(LIGHT_MAX_SEGS, cfg.configs[0].n_segs);
}

// ---------------------------------------------------------------------------
// Gamma LUT correctness
// ---------------------------------------------------------------------------

void test_gamma_lut_endpoints(void) {
    light_cfg_t cfg;
    light_cfg_parse(MINIMAL_CFG, &cfg);  // gamma=1.0 in MINIMAL_CFG
    TEST_ASSERT_EQUAL_UINT8(0,   cfg.gamma_lut[0]);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.gamma_lut[255]);
}

void test_gamma_1_is_linear(void) {
    light_cfg_t cfg;
    light_cfg_parse(MINIMAL_CFG, &cfg);  // gamma=1.0
    // With gamma=1.0, pow(i/255,1)*255 = i, so lut[i] should equal i.
    for (int i = 0; i < 256; i++)
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, cfg.gamma_lut[i]);
}

void test_gamma_2_2_midpoint(void) {
    // With gamma=2.2, lut[128] ≈ pow(128/255, 2.2)*255 ≈ 55
    const char *cfg_text =
        "[global]\ngamma=2.2\n"
        "[config]\nname=x\nstart=0\ncount=1\nr=0\ng=0\nb=0\nscale=100\n";
    light_cfg_t cfg;
    light_cfg_parse(cfg_text, &cfg);
    float expected = powf(128.0f / 255.0f, 2.2f) * 255.0f + 0.5f;
    TEST_ASSERT_UINT8_WITHIN(1, (uint8_t)expected, cfg.gamma_lut[128]);
}

void test_gamma_lut_monotonic(void) {
    const char *cfg_text =
        "[global]\ngamma=2.2\n"
        "[config]\nname=x\nstart=0\ncount=1\nr=0\ng=0\nb=0\nscale=100\n";
    light_cfg_t cfg;
    light_cfg_parse(cfg_text, &cfg);
    for (int i = 1; i < 256; i++)
        TEST_ASSERT_TRUE(cfg.gamma_lut[i] >= cfg.gamma_lut[i-1]);
}

// ---------------------------------------------------------------------------
// Fallback
// ---------------------------------------------------------------------------

void test_fallback_produces_valid_config(void) {
    light_cfg_t cfg;
    light_cfg_fallback(&cfg);
    TEST_ASSERT_EQUAL_UINT16(LIGHT_MAX_PIXELS, cfg.total_pixels);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("default", cfg.configs[0].name);
    TEST_ASSERT_EQUAL_UINT8(1,          cfg.configs[0].n_segs);
    TEST_ASSERT_EQUAL_UINT16(0,                         cfg.configs[0].segs[0].start);
    TEST_ASSERT_EQUAL_UINT16(LIGHT_MAX_PIXELS,          cfg.configs[0].segs[0].count);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.configs[0].segs[0].r);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.configs[0].segs[0].g);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.configs[0].segs[0].b);
    TEST_ASSERT_EQUAL_UINT8(20,  cfg.configs[0].segs[0].scale_pct);
    TEST_ASSERT_EQUAL_UINT8(0,   cfg.gamma_lut[0]);
    TEST_ASSERT_EQUAL_UINT8(255, cfg.gamma_lut[255]);
}

// ---------------------------------------------------------------------------
// light_cfg_load — uses the HAL mock
// ---------------------------------------------------------------------------

void test_load_returns_false_on_sd_fail(void) {
    hal_mock_set_sd_file(NULL);  // simulate read failure
    light_cfg_t cfg;
    bool ok = light_cfg_load(&cfg);
    TEST_ASSERT_FALSE(ok);
    // should fall back to the built-in default
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("default", cfg.configs[0].name);
}

void test_load_succeeds_with_valid_file(void) {
    hal_mock_set_sd_file(MINIMAL_CFG);
    light_cfg_t cfg;
    bool ok = light_cfg_load(&cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("test", cfg.configs[0].name);
}

void test_load_falls_back_on_parse_error(void) {
    hal_mock_set_sd_file("[global]\ntotal_pixels=10\n; no [config] section\n");
    light_cfg_t cfg;
    bool ok = light_cfg_load(&cfg);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.n_configs);
    TEST_ASSERT_EQUAL_STRING("default", cfg.configs[0].name);
}

// ---------------------------------------------------------------------------
// Parse: comments and blank lines ignored
// ---------------------------------------------------------------------------

void test_parse_ignores_comments_and_blanks(void) {
    const char *cfg_text =
        "; this is a comment\n"
        "# also a comment\n"
        "\n"
        "[global]\n"
        "; another comment\n"
        "total_pixels = 5\n"
        "\n"
        "[config]\n"
        "name = ok\n"
        "start = 0\n"
        "count = 5\n"
        "r = 1\ng = 2\nb = 3\nscale = 100\n";
    light_cfg_t cfg;
    bool ok = light_cfg_parse(cfg_text, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT16(5, cfg.total_pixels);
    TEST_ASSERT_EQUAL_STRING("ok", cfg.configs[0].name);
}

// ---------------------------------------------------------------------------
// Parse: case-insensitive keys
// ---------------------------------------------------------------------------

void test_parse_case_insensitive_keys(void) {
    const char *cfg_text =
        "[GLOBAL]\n"
        "Total_Pixels = 12\n"
        "[CONFIG]\n"
        "Name = ci_test\n"
        "Start = 0\nCount = 12\nR = 10\nG = 20\nB = 30\nScale = 75\n";
    light_cfg_t cfg;
    bool ok = light_cfg_parse(cfg_text, &cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT16(12, cfg.total_pixels);
    TEST_ASSERT_EQUAL_STRING("ci_test", cfg.configs[0].name);
    TEST_ASSERT_EQUAL_UINT8(75, cfg.configs[0].segs[0].scale_pct);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_global_fields);
    RUN_TEST(test_parse_config_name_and_segment);
    RUN_TEST(test_parse_two_segments);
    RUN_TEST(test_parse_two_configs);
    RUN_TEST(test_parse_no_config_returns_false);
    RUN_TEST(test_parse_empty_returns_false);
    RUN_TEST(test_parse_total_pixels_clamped);
    RUN_TEST(test_parse_overflow_configs_capped);
    RUN_TEST(test_parse_overflow_segs_capped);
    RUN_TEST(test_gamma_lut_endpoints);
    RUN_TEST(test_gamma_1_is_linear);
    RUN_TEST(test_gamma_2_2_midpoint);
    RUN_TEST(test_gamma_lut_monotonic);
    RUN_TEST(test_fallback_produces_valid_config);
    RUN_TEST(test_load_returns_false_on_sd_fail);
    RUN_TEST(test_load_succeeds_with_valid_file);
    RUN_TEST(test_load_falls_back_on_parse_error);
    RUN_TEST(test_parse_ignores_comments_and_blanks);
    RUN_TEST(test_parse_case_insensitive_keys);

    return UNITY_END();
}
