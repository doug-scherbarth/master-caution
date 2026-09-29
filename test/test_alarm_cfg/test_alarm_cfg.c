// test/test_alarm_cfg/test_alarm_cfg.c
// Unity tests for the alarm_cfg module.
// Run: pio test -e host -f test_alarm_cfg

#include <unity.h>
#include <string.h>
#include "alarm_cfg.h"
#include "channel_table.h"
#include "alarm_table.h"

void hal_mock_reset(void);
void hal_mock_set_file(const char *content);

static alarm_cfg_t cfg;

void setUp(void)    { hal_mock_reset(); memset(&cfg, 0, sizeof(cfg)); }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Fallback defaults
// ---------------------------------------------------------------------------

void test_fallback_all_active_high(void) {
    alarm_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_TRUE_MESSAGE(cfg.ch[i].active_high, CHANNEL_TABLE[i].name);
}

void test_fallback_all_pull_down(void) {
    alarm_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(ALARM_PULL_DOWN, cfg.ch[i].pull, CHANNEL_TABLE[i].name);
}

void test_fallback_debounce_matches_channel_table(void) {
    alarm_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(CHANNEL_TABLE[i].debounce_ms,
                                  cfg.ch[i].debounce_ms,
                                  CHANNEL_TABLE[i].name);
}

// ---------------------------------------------------------------------------
// Load status
// ---------------------------------------------------------------------------

void test_load_no_file_returns_no_file(void) {
    alarm_cfg_status_t s = alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(ALARM_CFG_NO_FILE, s);
}

void test_load_no_file_uses_fallback(void) {
    alarm_cfg_load(&cfg);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].active_high);
    TEST_ASSERT_EQUAL(ALARM_PULL_DOWN, cfg.ch[CH_CO_DETECT].pull);
}

void test_load_file_returns_ok(void) {
    hal_mock_set_file("[defaults]\nactive_high = yes\n");
    alarm_cfg_status_t s = alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(ALARM_CFG_OK, s);
}

void test_load_empty_file_returns_no_file(void) {
    hal_mock_set_file("");
    alarm_cfg_status_t s = alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(ALARM_CFG_NO_FILE, s);
}

// ---------------------------------------------------------------------------
// [defaults] section
// ---------------------------------------------------------------------------

void test_defaults_active_low_sets_all(void) {
    hal_mock_set_file("[defaults]\nactive_high = no\n");
    alarm_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_FALSE_MESSAGE(cfg.ch[i].active_high, CHANNEL_TABLE[i].name);
}

void test_defaults_pull_up_sets_all(void) {
    hal_mock_set_file("[defaults]\npull = up\n");
    alarm_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(ALARM_PULL_UP, cfg.ch[i].pull, CHANNEL_TABLE[i].name);
}

void test_defaults_pull_none_sets_all(void) {
    hal_mock_set_file("[defaults]\npull = none\n");
    alarm_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(ALARM_PULL_NONE, cfg.ch[i].pull, CHANNEL_TABLE[i].name);
}

void test_defaults_debounce_valid_range(void) {
    hal_mock_set_file("[defaults]\ndebounce_ms = 300\n");
    alarm_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(300, cfg.ch[i].debounce_ms, CHANNEL_TABLE[i].name);
}

// ---------------------------------------------------------------------------
// Debounce range validation (per-key fallback)
// ---------------------------------------------------------------------------

void test_debounce_too_small_keeps_fallback(void) {
    hal_mock_set_file(
        "[CO_DETECT]\n"
        "debounce_ms = 49\n"  // below 50ms minimum
    );
    alarm_cfg_load(&cfg);
    // Should keep the compiled default for CO_DETECT
    TEST_ASSERT_EQUAL(CHANNEL_TABLE[CH_CO_DETECT].debounce_ms, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_debounce_too_large_keeps_fallback(void) {
    hal_mock_set_file(
        "[CO_DETECT]\n"
        "debounce_ms = 5001\n"  // above 5000ms maximum
    );
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(CHANNEL_TABLE[CH_CO_DETECT].debounce_ms, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_debounce_boundary_low_accepted(void) {
    hal_mock_set_file("[CO_DETECT]\ndebounce_ms = 50\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(50, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_debounce_boundary_high_accepted(void) {
    hal_mock_set_file("[CO_DETECT]\ndebounce_ms = 5000\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(5000, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_debounce_out_of_range_does_not_affect_other_channels(void) {
    hal_mock_set_file("[CO_DETECT]\ndebounce_ms = 10\n");
    alarm_cfg_load(&cfg);
    // Other channels should still have fallback debounce
    TEST_ASSERT_EQUAL(CHANNEL_TABLE[CH_CHT_OVERTEMP].debounce_ms, cfg.ch[CH_CHT_OVERTEMP].debounce_ms);
}

// ---------------------------------------------------------------------------
// Named channel section overrides
// ---------------------------------------------------------------------------

void test_named_section_active_low(void) {
    hal_mock_set_file("[FUEL_PRESS_LOW]\nactive_high = no\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_FALSE(cfg.ch[CH_FUEL_PRESS_LOW].active_high);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].active_high);  // others unaffected
}

void test_named_section_pull_up(void) {
    hal_mock_set_file("[FUEL_PRESS_LOW]\npull = up\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(ALARM_PULL_UP,   cfg.ch[CH_FUEL_PRESS_LOW].pull);
    TEST_ASSERT_EQUAL(ALARM_PULL_DOWN, cfg.ch[CH_CO_DETECT].pull);
}

void test_named_section_debounce_override(void) {
    hal_mock_set_file("[FLAPS_DEPLOYED]\ndebounce_ms = 200\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(200, cfg.ch[CH_FLAPS_DEPLOYED].debounce_ms);
    TEST_ASSERT_EQUAL(CHANNEL_TABLE[CH_CO_DETECT].debounce_ms, cfg.ch[CH_CO_DETECT].debounce_ms);
}

void test_defaults_then_named_override(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "debounce_ms = 1000\n"
        "[CHT_OVERTEMP]\n"
        "debounce_ms = 75\n"
    );
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(1000, cfg.ch[CH_CO_DETECT].debounce_ms);
    TEST_ASSERT_EQUAL(75,   cfg.ch[CH_CHT_OVERTEMP].debounce_ms);
}

void test_defaults_then_named_polarity(void) {
    hal_mock_set_file(
        "[defaults]\n"
        "active_high = no\n"
        "pull = up\n"
        "[CO_DETECT]\n"
        "active_high = yes\n"
        "pull = down\n"
    );
    alarm_cfg_load(&cfg);
    // CO_DETECT overrides defaults back to active_high/pulldown
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].active_high);
    TEST_ASSERT_EQUAL(ALARM_PULL_DOWN, cfg.ch[CH_CO_DETECT].pull);
    // Others stay with the defaults values
    TEST_ASSERT_FALSE(cfg.ch[CH_CHT_OVERTEMP].active_high);
    TEST_ASSERT_EQUAL(ALARM_PULL_UP, cfg.ch[CH_CHT_OVERTEMP].pull);
}

// ---------------------------------------------------------------------------
// Robustness
// ---------------------------------------------------------------------------

// Spec §2.3: severity is compiled in, never SD-configurable.
// A severity = key is treated as unknown; one warning is produced.
// Spec §2.3: wav_id override is range-checked; 0..WAV_COUNT-1 accepted, else warn.

void test_wav_id_valid_stored_in_override(void) {
    alarm_cfg_fallback(&cfg);
    uint8_t warns = alarm_cfg_parse("[CO_DETECT]\nwav_id = 5\n", &cfg);
    TEST_ASSERT_EQUAL(0, warns);
    TEST_ASSERT_EQUAL(5, cfg.ch[CH_CO_DETECT].wav_id_override);
}

void test_wav_id_out_of_range_warns_and_keeps_default(void) {
    alarm_cfg_fallback(&cfg);
    uint8_t warns = alarm_cfg_parse("[CO_DETECT]\nwav_id = 99\n", &cfg);
    TEST_ASSERT_GREATER_OR_EQUAL(1, warns);
    TEST_ASSERT_EQUAL(0xFF, cfg.ch[CH_CO_DETECT].wav_id_override);  // unchanged
}

void test_wav_id_zero_is_valid(void) {
    alarm_cfg_fallback(&cfg);
    uint8_t warns = alarm_cfg_parse("[CO_DETECT]\nwav_id = 0\n", &cfg);
    TEST_ASSERT_EQUAL(0, warns);
    TEST_ASSERT_EQUAL(0, cfg.ch[CH_CO_DETECT].wav_id_override);
}

void test_wav_id_boundary_max_valid(void) {
    alarm_cfg_fallback(&cfg);
    char buf[64];
    // WAV_COUNT - 1 is the maximum valid id
    snprintf(buf, sizeof(buf), "[CO_DETECT]\nwav_id = %d\n", (int)(WAV_COUNT - 1));
    uint8_t warns = alarm_cfg_parse(buf, &cfg);
    TEST_ASSERT_EQUAL(0, warns);
    TEST_ASSERT_EQUAL((uint8_t)(WAV_COUNT - 1), cfg.ch[CH_CO_DETECT].wav_id_override);
}

void test_wav_id_in_defaults_section_silently_ignored(void) {
    // wav_id in [defaults] is silently ignored (defaults pass never warns).
    // Verify no channel's override was accidentally set.
    alarm_cfg_fallback(&cfg);
    uint8_t warns = alarm_cfg_parse("[defaults]\nwav_id = 2\n", &cfg);
    TEST_ASSERT_EQUAL(0, warns);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(0xFF, cfg.ch[i].wav_id_override, CHANNEL_TABLE[i].name);
}

void test_wav_id_fallback_is_0xFF(void) {
    alarm_cfg_fallback(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_EQUAL_MESSAGE(0xFF, cfg.ch[i].wav_id_override, CHANNEL_TABLE[i].name);
}

void test_severity_key_counts_as_unknown_key_warn(void) {
    alarm_cfg_fallback(&cfg);
    uint8_t warns = alarm_cfg_parse("[CO_DETECT]\nseverity = low\n", &cfg);
    TEST_ASSERT_GREATER_OR_EQUAL(1, warns);
    // Channel config unchanged from defaults (severity has no channel-level effect).
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].active_high);
    TEST_ASSERT_EQUAL(ALARM_PULL_DOWN, cfg.ch[CH_CO_DETECT].pull);
}

void test_unknown_section_ignored(void) {
    hal_mock_set_file("[BOGUS_CHANNEL]\nactive_high = no\n");
    alarm_cfg_load(&cfg);
    for (int i = 0; i < CHANNEL_COUNT; i++)
        TEST_ASSERT_TRUE_MESSAGE(cfg.ch[i].active_high, CHANNEL_TABLE[i].name);
}

void test_case_insensitive_section(void) {
    hal_mock_set_file("[oil_press_low]\nactive_high = no\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_FALSE(cfg.ch[CH_OIL_PRESS_LOW].active_high);
}

void test_unknown_pull_value_keeps_fallback(void) {
    hal_mock_set_file("[CO_DETECT]\npull = sideways\n");
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(ALARM_PULL_DOWN, cfg.ch[CH_CO_DETECT].pull);
}

void test_comments_and_blanks_ignored(void) {
    hal_mock_set_file(
        "; comment\n"
        "# another comment\n"
        "\n"
        "[FLAPS_DEPLOYED]\n"
        "debounce_ms = 100\n"
    );
    alarm_cfg_load(&cfg);
    TEST_ASSERT_EQUAL(100, cfg.ch[CH_FLAPS_DEPLOYED].debounce_ms);
    TEST_ASSERT_TRUE(cfg.ch[CH_CO_DETECT].active_high);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fallback_all_active_high);
    RUN_TEST(test_fallback_all_pull_down);
    RUN_TEST(test_fallback_debounce_matches_channel_table);
    RUN_TEST(test_load_no_file_returns_no_file);
    RUN_TEST(test_load_no_file_uses_fallback);
    RUN_TEST(test_load_file_returns_ok);
    RUN_TEST(test_load_empty_file_returns_no_file);
    RUN_TEST(test_defaults_active_low_sets_all);
    RUN_TEST(test_defaults_pull_up_sets_all);
    RUN_TEST(test_defaults_pull_none_sets_all);
    RUN_TEST(test_defaults_debounce_valid_range);
    RUN_TEST(test_debounce_too_small_keeps_fallback);
    RUN_TEST(test_debounce_too_large_keeps_fallback);
    RUN_TEST(test_debounce_boundary_low_accepted);
    RUN_TEST(test_debounce_boundary_high_accepted);
    RUN_TEST(test_debounce_out_of_range_does_not_affect_other_channels);
    RUN_TEST(test_named_section_active_low);
    RUN_TEST(test_named_section_pull_up);
    RUN_TEST(test_named_section_debounce_override);
    RUN_TEST(test_defaults_then_named_override);
    RUN_TEST(test_defaults_then_named_polarity);
    RUN_TEST(test_wav_id_fallback_is_0xFF);
    RUN_TEST(test_wav_id_valid_stored_in_override);
    RUN_TEST(test_wav_id_out_of_range_warns_and_keeps_default);
    RUN_TEST(test_wav_id_zero_is_valid);
    RUN_TEST(test_wav_id_boundary_max_valid);
    RUN_TEST(test_wav_id_in_defaults_section_silently_ignored);
    RUN_TEST(test_severity_key_counts_as_unknown_key_warn);
    RUN_TEST(test_unknown_section_ignored);
    RUN_TEST(test_case_insensitive_section);
    RUN_TEST(test_unknown_pull_value_keeps_fallback);
    RUN_TEST(test_comments_and_blanks_ignored);
    return UNITY_END();
}
