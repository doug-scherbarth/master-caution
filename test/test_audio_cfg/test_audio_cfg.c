// test/test_audio_cfg/test_audio_cfg.c
// Unity tests for audio_cfg parser and loader.
// Run on the host: pio test -e host -f test_audio_cfg

#include <unity.h>
#include "audio_cfg.h"

void hal_mock_reset(void);
void hal_mock_set_sd_file(const char *content);

void setUp(void)    { hal_mock_reset(); }
void tearDown(void) {}

// --- audio_cfg_parse() ---

void test_empty_text_gives_defaults(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_DEFAULT, cfg.gain);
}

void test_null_text_gives_defaults(void) {
    audio_cfg_t cfg;
    audio_cfg_parse(NULL, &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_DEFAULT, cfg.gain);
}

void test_parse_explicit_gain(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = 0.80\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.80f, cfg.gain);
}

void test_parse_unity_gain(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = 1.0\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, cfg.gain);
}

void test_parse_mute(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = 0.0\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg.gain);
}

void test_gain_above_max_clamped(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = 5.0\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_MAX, cfg.gain);
}

void test_gain_below_zero_clamped_to_zero(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = -1.0\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg.gain);
}

void test_non_numeric_gain_keeps_default(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = loud\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_DEFAULT, cfg.gain);
}

void test_inline_comment_stripped(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\ngain = 0.50 ; half volume\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.50f, cfg.gain);
}

void test_keys_case_insensitive(void) {
    audio_cfg_t cfg;
    audio_cfg_parse("[global]\nGAIN = 1.5\n", &cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, cfg.gain);
}

// --- audio_cfg_load() ---

void test_load_missing_file_uses_defaults(void) {
    // SD returns NOT_FOUND
    audio_cfg_t cfg;
    bool ok = audio_cfg_load(&cfg);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_DEFAULT, cfg.gain);
}

void test_load_valid_file(void) {
    hal_mock_set_sd_file("[global]\ngain = 1.20\n");
    audio_cfg_t cfg;
    bool ok = audio_cfg_load(&cfg);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.20f, cfg.gain);
}

void test_load_too_big_returns_false_and_defaults(void) {
    // Build a string longer than AUDIO_CFG_FILE_MAX (2048 bytes).
    static char big[2100];
    memset(big, ' ', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    hal_mock_set_sd_file(big);
    audio_cfg_t cfg;
    bool ok = audio_cfg_load(&cfg);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, AUDIO_CFG_GAIN_DEFAULT, cfg.gain);
}

#include <string.h>

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_text_gives_defaults);
    RUN_TEST(test_null_text_gives_defaults);
    RUN_TEST(test_parse_explicit_gain);
    RUN_TEST(test_parse_unity_gain);
    RUN_TEST(test_parse_mute);
    RUN_TEST(test_gain_above_max_clamped);
    RUN_TEST(test_gain_below_zero_clamped_to_zero);
    RUN_TEST(test_non_numeric_gain_keeps_default);
    RUN_TEST(test_inline_comment_stripped);
    RUN_TEST(test_keys_case_insensitive);
    RUN_TEST(test_load_missing_file_uses_defaults);
    RUN_TEST(test_load_valid_file);
    RUN_TEST(test_load_too_big_returns_false_and_defaults);
    return UNITY_END();
}
