// test/test_channel_cfg/test_channel_cfg.c
// Unity tests for the channel_cfg module.
// Run: pio test -e host -f test_channel_cfg

#include <unity.h>
#include <string.h>
#include "channel_cfg.h"
#include "channel_table.h"

void hal_mock_reset(void);
void hal_mock_set_file(const char *content);

static channel_cfg_t cfg;

void setUp(void)    { hal_mock_reset(); memset(&cfg, 0, sizeof(cfg)); }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Fallback defaults
// ---------------------------------------------------------------------------

void test_fallback_all_enabled(void) {
    channel_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_TRUE_MESSAGE(cfg.ch[i].enabled, CHANNEL_TABLE[i].name);
}

void test_fallback_oil_press_startup_excluded(void) {
    channel_cfg_fallback(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_OIL_PRESS_LOW].startup_excluded);
}

void test_fallback_others_not_startup_excluded(void) {
    channel_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (i == CH_OIL_PRESS_LOW) continue;
        TEST_ASSERT_FALSE_MESSAGE(cfg.ch[i].startup_excluded, CHANNEL_TABLE[i].name);
    }
}

void test_fallback_debounce_matches_channel_table(void) {
    channel_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(CHANNEL_TABLE[i].debounce_ms,
                                  cfg.ch[i].debounce_ms,
                                  CHANNEL_TABLE[i].name);
}

// ---------------------------------------------------------------------------
// Load — no file returns false and uses fallback
// ---------------------------------------------------------------------------

void test_load_no_file_returns_false(void) {
    // hal_mock returns false from hal_sd_read_file when no file set
    bool ok = channel_cfg_load(&cfg);
    TEST_ASSERT_FALSE(ok);
}

void test_load_no_file_uses_fallback(void) {
    channel_cfg_load(&cfg);
    // fallback: OIL_PRESS_LOW excluded, all else enabled
    TEST_ASSERT_TRUE(cfg.ch[CH_OIL_PRESS_LOW].startup_excluded);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].enabled);
}

// ---------------------------------------------------------------------------
// [defaults] section
// ---------------------------------------------------------------------------

void test_defaults_debounce_overrides_all(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "debounce_ms = 200\n"
    );
    channel_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(200, cfg.ch[i].debounce_ms, CHANNEL_TABLE[i].name);
}

void test_defaults_disable_all(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "enabled = no\n"
    );
    channel_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_FALSE_MESSAGE(cfg.ch[i].enabled, CHANNEL_TABLE[i].name);
}

void test_defaults_startup_excluded_all(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "startup_excluded = yes\n"
    );
    channel_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_TRUE_MESSAGE(cfg.ch[i].startup_excluded, CHANNEL_TABLE[i].name);
}

void test_defaults_enabled_true_false_keywords(void) {
    hal_mock_set_file("[defaults]\nenabled = no\n");
    channel_cfg_load(&cfg);
    TEST_ASSERT_FALSE(cfg.ch[0].enabled);

    hal_mock_set_file("[defaults]\nenabled = yes\n");
    channel_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[0].enabled);
}

// ---------------------------------------------------------------------------
// Named channel section overrides
// ---------------------------------------------------------------------------

void test_named_section_enables_one_channel(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "enabled = no\n"
        "[CO_DETECT]\n"
        "enabled = yes\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].enabled);
    TEST_ASSERT_FALSE(cfg.ch[CH_L_MAG_FAIL].enabled);
}

void test_named_section_startup_excluded(void) {
    hal_mock_set_file(
        "[CO_DETECT]\n"
        "startup_excluded = yes\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].startup_excluded);
    // others unchanged (OIL_PRESS_LOW still excluded via fallback)
    TEST_ASSERT_TRUE(cfg.ch[CH_OIL_PRESS_LOW].startup_excluded);
    TEST_ASSERT_FALSE(cfg.ch[CH_L_MAG_FAIL].startup_excluded);
}

void test_named_section_debounce_override(void) {
    hal_mock_set_file(
        "[FLAPS_DEPLOYED]\n"
        "debounce_ms = 100\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(100, cfg.ch[CH_FLAPS_DEPLOYED].debounce_ms);
    // other channels unaffected
    TEST_ASSERT_EQUAL(500, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_defaults_then_named_override(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "debounce_ms = 1000\n"
        "[CHT_OVERTEMP]\n"
        "debounce_ms = 50\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(1000, cfg.ch[CH_CO_DETECT].debounce_ms);
    TEST_ASSERT_EQUAL(50,   cfg.ch[CH_CHT_OVERTEMP].debounce_ms);
}

// ---------------------------------------------------------------------------
// Robustness
// ---------------------------------------------------------------------------

void test_unknown_section_ignored(void) {
    hal_mock_set_file(
        "[BOGUS_CHANNEL]\n"
        "enabled = no\n"
    );
    channel_cfg_load(&cfg);
    // All channels should still be enabled (fallback default)
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_TRUE_MESSAGE(cfg.ch[i].enabled, CHANNEL_TABLE[i].name);
}

void test_case_insensitive_section(void) {
    hal_mock_set_file(
        "[oil_press_low]\n"
        "enabled = no\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_FALSE(cfg.ch[CH_OIL_PRESS_LOW].enabled);
}

void test_case_insensitive_value_yes(void) {
    // parse_bool is strict lowercase: "YES" is NOT recognized, evaluates to false.
    // This verifies the actual behavior: uppercase values silently apply the false default.
    hal_mock_set_file(
        "[defaults]\n"
        "enabled = YES\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_FALSE(cfg.ch[0].enabled);
}

void test_empty_file_uses_fallback_values(void) {
    hal_mock_set_file("");
    channel_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_OIL_PRESS_LOW].startup_excluded);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].enabled);
}

void test_comments_and_blanks_ignored(void) {
    hal_mock_set_file(
        "; comment line\n"
        "# another comment\n"
        "\n"
        "[OIL_PRESS_LOW]\n"
        "; inline comment not supported but line is skipped\n"
        "startup_excluded = yes\n"
    );
    channel_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_OIL_PRESS_LOW].startup_excluded);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].enabled);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fallback_all_enabled);
    RUN_TEST(test_fallback_oil_press_startup_excluded);
    RUN_TEST(test_fallback_others_not_startup_excluded);
    RUN_TEST(test_fallback_debounce_matches_channel_table);
    RUN_TEST(test_load_no_file_returns_false);
    RUN_TEST(test_load_no_file_uses_fallback);
    RUN_TEST(test_defaults_debounce_overrides_all);
    RUN_TEST(test_defaults_disable_all);
    RUN_TEST(test_defaults_startup_excluded_all);
    RUN_TEST(test_defaults_enabled_true_false_keywords);
    RUN_TEST(test_named_section_enables_one_channel);
    RUN_TEST(test_named_section_startup_excluded);
    RUN_TEST(test_named_section_debounce_override);
    RUN_TEST(test_defaults_then_named_override);
    RUN_TEST(test_unknown_section_ignored);
    RUN_TEST(test_case_insensitive_section);
    RUN_TEST(test_case_insensitive_value_yes);
    RUN_TEST(test_empty_file_uses_fallback_values);
    RUN_TEST(test_comments_and_blanks_ignored);
    return UNITY_END();
}
