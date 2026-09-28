// test/test_pixel_lighting/test_pixel_lighting.c
// Unity tests for the pixel_lighting module.
// Run: pio test -e host -f test_pixel_lighting

#include <unity.h>
#include "pixel_lighting.h"
#include "light_cfg.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

void hal_mock_reset(void);
void hal_mock_set_adc(hal_adc_ch_t ch, uint16_t val);
const uint8_t *hal_mock_get_pixels(void);
uint16_t       hal_mock_get_n_pixels(void);
int            hal_mock_pixels_write_count(void);

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

// Single solid-red segment, gamma=1 (linear) for predictable math
static const char CFG_RED_LINEAR[] =
    "[global]\n"
    "total_pixels = 10\n"
    "mA_per_channel = 20\n"
    "quiescent_mA = 1\n"
    "max_current_mA = 9999\n"
    "gamma = 1.0\n"
    "[config]\n"
    "name = red\n"
    "start = 0\n"
    "count = 10\n"
    "r = 255\n"
    "g = 0\n"
    "b = 0\n"
    "scale = 100\n";

// Two-segment config: 5 red + 5 blue
static const char CFG_TWO_SEG[] =
    "[global]\n"
    "total_pixels = 10\n"
    "mA_per_channel = 20\n"
    "quiescent_mA = 1\n"
    "max_current_mA = 9999\n"
    "gamma = 1.0\n"
    "[config]\n"
    "name = rb\n"
    "start = 0\n"
    "count = 5\n"
    "r = 255\ng = 0\nb = 0\nscale = 100\n"
    "start = 5\n"
    "count = 5\n"
    "r = 0\ng = 0\nb = 255\nscale = 100\n";

// Config that exceeds max_current_mA to trigger current limiter
// 10 pixels × (255/255×20mA + 1mA quiescent) = 210 mA but we cap at 100 mA
static const char CFG_CURRENT_LIMIT[] =
    "[global]\n"
    "total_pixels = 10\n"
    "mA_per_channel = 20\n"
    "quiescent_mA = 1\n"
    "max_current_mA = 50\n"
    "gamma = 1.0\n"
    "[config]\n"
    "name = bright\n"
    "start = 0\n"
    "count = 10\n"
    "r = 255\ng = 255\nb = 255\nscale = 100\n";

static light_cfg_t s_cfg;

void setUp(void)    { hal_mock_reset(); }
void tearDown(void) {}

static void load(const char *text) {
    memset(&s_cfg, 0, sizeof(s_cfg));
    light_cfg_parse(text, &s_cfg);
    pixel_lighting_init(&s_cfg, 0);
}

// ---------------------------------------------------------------------------
// Basic rendering
// ---------------------------------------------------------------------------

void test_tick_calls_pixels_write(void) {
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0, 255);
    TEST_ASSERT_EQUAL_INT(1, hal_mock_pixels_write_count());
}

void test_tick_writes_correct_pixel_count(void) {
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0, 255);
    TEST_ASSERT_EQUAL_UINT16(10, hal_mock_get_n_pixels());
}

void test_full_dimmer_full_red(void) {
    // brightness=255 → gamma1 → 255; r=255,scale=100 → 255
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0, 255);
    const uint8_t *px = hal_mock_get_pixels();
    TEST_ASSERT_EQUAL_UINT8(255, px[0]);  // R
    TEST_ASSERT_EQUAL_UINT8(0,   px[1]);  // G
    TEST_ASSERT_EQUAL_UINT8(0,   px[2]);  // B
}

void test_zero_dimmer_all_black(void) {
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0, 0);
    const uint8_t *px = hal_mock_get_pixels();
    for (int i = 0; i < 10 * 3; i++)
        TEST_ASSERT_EQUAL_UINT8(0, px[i]);
}

void test_half_dimmer_approx_half_brightness(void) {
    // brightness=128 (DIM_IN=2048,BUS=4095 ratiometric) → gamma1 → 128
    // r = 255 × 100/100 × 128/255 = 128
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0, 128);
    const uint8_t *px = hal_mock_get_pixels();
    TEST_ASSERT_UINT8_WITHIN(2, 128, px[0]);
}

void test_two_segment_rendering(void) {
    load(CFG_TWO_SEG);
    pixel_lighting_tick(0, 255);
    const uint8_t *px = hal_mock_get_pixels();
    // pixels 0-4: red
    TEST_ASSERT_EQUAL_UINT8(255, px[0*3+0]);
    TEST_ASSERT_EQUAL_UINT8(0,   px[0*3+1]);
    TEST_ASSERT_EQUAL_UINT8(0,   px[0*3+2]);
    TEST_ASSERT_EQUAL_UINT8(255, px[4*3+0]);
    // pixels 5-9: blue
    TEST_ASSERT_EQUAL_UINT8(0,   px[5*3+0]);
    TEST_ASSERT_EQUAL_UINT8(0,   px[5*3+1]);
    TEST_ASSERT_EQUAL_UINT8(255, px[5*3+2]);
    TEST_ASSERT_EQUAL_UINT8(255, px[9*3+2]);
}

// ---------------------------------------------------------------------------
// Scale_pct
// ---------------------------------------------------------------------------

void test_scale_50_halves_brightness(void) {
    const char *cfg_text =
        "[global]\ntotal_pixels=4\nmA_per_channel=20\nquiescent_mA=1\n"
        "max_current_mA=9999\ngamma=1.0\n"
        "[config]\nname=half\nstart=0\ncount=4\nr=200\ng=0\nb=0\nscale=50\n";
    load(cfg_text);
    pixel_lighting_tick(0, 255);
    const uint8_t *px = hal_mock_get_pixels();
    // 200 × 50/100 × 255/255 = 100
    TEST_ASSERT_UINT8_WITHIN(2, 100, px[0]);
}

// ---------------------------------------------------------------------------
// Current limiter
// ---------------------------------------------------------------------------

void test_current_limit_reduces_output(void) {
    // At full brightness, 10 white pixels: sum = 10×(255+255+255) = 7650
    // est_ma = 7650×20/255 + 10×1 = 600 + 10 = 610, cap = 50
    // So every channel should be scaled down by 50/610 ≈ 0.082
    load(CFG_CURRENT_LIMIT);
    pixel_lighting_tick(0, 255);
    const uint8_t *px = hal_mock_get_pixels();
    // After scaling, each channel should be < 30 (well below 255)
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(px[i*3+0] < 30);
        TEST_ASSERT_TRUE(px[i*3+1] < 30);
        TEST_ASSERT_TRUE(px[i*3+2] < 30);
    }
}

void test_below_current_limit_no_reduction(void) {
    // brightness=0 → all pixels zero → no scaling applied
    load(CFG_CURRENT_LIMIT);
    pixel_lighting_tick(0, 0);
    const uint8_t *px = hal_mock_get_pixels();
    for (int i = 0; i < 30; i++) TEST_ASSERT_EQUAL_UINT8(0, px[i]);
}

// ---------------------------------------------------------------------------
// Config switching
// ---------------------------------------------------------------------------

void test_set_config_switches_active_config(void) {
    const char *two =
        "[global]\ntotal_pixels=4\nmA_per_channel=20\nquiescent_mA=1\n"
        "max_current_mA=9999\ngamma=1.0\n"
        "[config]\nname=red\nstart=0\ncount=4\nr=255\ng=0\nb=0\nscale=100\n"
        "[config]\nname=blue\nstart=0\ncount=4\nr=0\ng=0\nb=255\nscale=100\n";
    load(two);

    pixel_lighting_set_config(1);
    TEST_ASSERT_EQUAL_UINT8(1, pixel_lighting_get_config());
    pixel_lighting_tick(0, 255);
    const uint8_t *px = hal_mock_get_pixels();
    TEST_ASSERT_EQUAL_UINT8(0,   px[0]);  // R=0
    TEST_ASSERT_EQUAL_UINT8(255, px[2]);  // B=255
}

void test_set_config_clamps_to_last(void) {
    load(CFG_RED_LINEAR);  // only 1 config
    uint8_t applied = pixel_lighting_set_config(99);
    TEST_ASSERT_EQUAL_UINT8(0, applied);
}

void test_get_config_default_zero(void) {
    load(CFG_RED_LINEAR);
    TEST_ASSERT_EQUAL_UINT8(0, pixel_lighting_get_config());
}

// ---------------------------------------------------------------------------
// max_ma helper
// ---------------------------------------------------------------------------

void test_max_ma_single_white_seg(void) {
    // 10 pixels white scale=100:
    // ch_ma = (255+255+255)×20/255 = 60 per pixel
    // total = (60+1) × 10 = 610
    load(CFG_CURRENT_LIMIT);
    uint32_t ma = pixel_lighting_max_ma(&s_cfg, 0);
    TEST_ASSERT_EQUAL_UINT32(610, ma);
}

void test_max_ma_zero_scale(void) {
    const char *cfg_text =
        "[global]\ntotal_pixels=4\nmA_per_channel=20\nquiescent_mA=1\n"
        "max_current_mA=9999\ngamma=1.0\n"
        "[config]\nname=off\nstart=0\ncount=4\nr=0\ng=0\nb=0\nscale=0\n";
    load(cfg_text);
    uint32_t ma = pixel_lighting_max_ma(&s_cfg, 0);
    // ch_ma = 0; total = (0+1)*4 = 4 quiescent only
    TEST_ASSERT_EQUAL_UINT32(4, ma);
}

void test_max_ma_invalid_config_idx_returns_zero(void) {
    load(CFG_RED_LINEAR);
    TEST_ASSERT_EQUAL_UINT32(0, pixel_lighting_max_ma(&s_cfg, 99));
}

// ---------------------------------------------------------------------------
// Gamma integration (non-linear LUT from parse with gamma=2.2)
// ---------------------------------------------------------------------------

void test_gamma_2_2_half_dimmer_darker_than_linear(void) {
    // With gamma=2.2, brightness≈128/255 produces a much darker output
    // than linear. Check the rendered value is significantly less than 128.
    const char *cfg_text =
        "[global]\ntotal_pixels=2\nmA_per_channel=20\nquiescent_mA=1\n"
        "max_current_mA=9999\ngamma=2.2\n"
        "[config]\nname=g\nstart=0\ncount=2\nr=255\ng=0\nb=0\nscale=100\n";
    load(cfg_text);
    pixel_lighting_tick(0, 128);
    const uint8_t *px = hal_mock_get_pixels();
    // linear would give 128; gamma=2.2 gives ~55 — check it's below 80
    TEST_ASSERT_TRUE(px[0] < 80);
    TEST_ASSERT_TRUE(px[0] > 0);
}

// ---------------------------------------------------------------------------
// Null / empty config guard
// ---------------------------------------------------------------------------

void test_tick_with_null_cfg_does_not_crash(void) {
    pixel_lighting_init(NULL, 0);
    pixel_lighting_tick(0, 255);  // must not crash; pixels_write should not be called
    TEST_ASSERT_EQUAL_INT(0, hal_mock_pixels_write_count());
}

// ---------------------------------------------------------------------------
// 50 Hz rate limiting
// ---------------------------------------------------------------------------

void test_renders_throttled_to_50hz(void) {
    load(CFG_RED_LINEAR);
    pixel_lighting_tick(0,  255);   // first render at t=0
    pixel_lighting_tick(5,  255);   // within 20 ms window — skipped
    pixel_lighting_tick(10, 255);   // still within window — skipped
    TEST_ASSERT_EQUAL_INT(1, hal_mock_pixels_write_count());
    pixel_lighting_tick(20, 255);   // new 20 ms window — renders
    TEST_ASSERT_EQUAL_INT(2, hal_mock_pixels_write_count());
}

void test_low_brightness_noise_tolerance(void) {
    // With gamma=2.2, brightness values 3, 5, 7 all map to 0 in the LUT,
    // demonstrating that ±2 LSB of ADC noise at a low setting is invisible.
    const char *cfg_text =
        "[global]\ntotal_pixels=4\nmA_per_channel=20\nquiescent_mA=1\n"
        "max_current_mA=9999\ngamma=2.2\n"
        "[config]\nname=g\nstart=0\ncount=4\nr=255\ng=0\nb=0\nscale=100\n";
    load(cfg_text);
    pixel_lighting_tick(0,  3);
    uint8_t r3 = hal_mock_get_pixels()[0];
    pixel_lighting_tick(20, 5);
    uint8_t r5 = hal_mock_get_pixels()[0];
    pixel_lighting_tick(40, 7);
    uint8_t r7 = hal_mock_get_pixels()[0];
    TEST_ASSERT_EQUAL_UINT8(r3, r5);
    TEST_ASSERT_EQUAL_UINT8(r5, r7);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tick_calls_pixels_write);
    RUN_TEST(test_tick_writes_correct_pixel_count);
    RUN_TEST(test_full_dimmer_full_red);
    RUN_TEST(test_zero_dimmer_all_black);
    RUN_TEST(test_half_dimmer_approx_half_brightness);
    RUN_TEST(test_two_segment_rendering);
    RUN_TEST(test_scale_50_halves_brightness);
    RUN_TEST(test_current_limit_reduces_output);
    RUN_TEST(test_below_current_limit_no_reduction);
    RUN_TEST(test_set_config_switches_active_config);
    RUN_TEST(test_set_config_clamps_to_last);
    RUN_TEST(test_get_config_default_zero);
    RUN_TEST(test_max_ma_single_white_seg);
    RUN_TEST(test_max_ma_zero_scale);
    RUN_TEST(test_max_ma_invalid_config_idx_returns_zero);
    RUN_TEST(test_gamma_2_2_half_dimmer_darker_than_linear);
    RUN_TEST(test_tick_with_null_cfg_does_not_crash);
    RUN_TEST(test_renders_throttled_to_50hz);
    RUN_TEST(test_low_brightness_noise_tolerance);

    return UNITY_END();
}
