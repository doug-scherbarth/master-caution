// test/test_regress_alarm_cfg/test_regress_alarm_cfg.c
// Regression for review item 3: ALARMS.CFG must never fail silently.
//
// API contract (TASKS.md, Tasks 1 and 3) -- this suite will NOT COMPILE until
// those API changes land. That is the expected red state.
//   hal_sd_status_t hal_sd_read_file(...)         HAL_SD_OK / NOT_FOUND / TOO_BIG / IO_ERROR
//   uint8_t alarm_cfg_parse(text, out)            returns number of warnings
//   ALARM_CFG_WARN                                new status: parsed, but with warnings
//   alarm_cfg_load: NOT_FOUND -> NO_FILE, TOO_BIG/IO_ERROR -> ERROR,
//                   warnings > 0 -> WARN, clean -> OK
//   Buffer >= 8 KB (a 6 KB commented file must load).
//   [defaults] applies before named sections regardless of position in file.

#include <unity.h>
#include <string.h>
#include "alarm_cfg.h"
#include "channel_table.h"
#include "hal.h"

void hal_mock_reset(void);
void hal_mock_set_file(const char *content);
void hal_mock_set_status(hal_sd_status_t st);

static alarm_cfg_t g_cfg;

void setUp(void)    { hal_mock_reset(); }
void tearDown(void) {}

// --- Parsing: values ---------------------------------------------------------

void test_inline_comment_stripped_from_value(void) {
    uint8_t w = alarm_cfg_parse("[OIL_PRESS_LOW]\n"
                                "active_high = no   ; NC switch\n"
                                "pull        = up   ; per wiring\n", &g_cfg);
    TEST_ASSERT_FALSE(g_cfg.ch[CH_OIL_PRESS_LOW].active_high);
    TEST_ASSERT_EQUAL(ALARM_PULL_UP, g_cfg.ch[CH_OIL_PRESS_LOW].pull);
    TEST_ASSERT_EQUAL_UINT8(0, w);
}

void test_debounce_overflow_rejected_not_wrapped(void) {
    uint8_t w = alarm_cfg_parse("[L_MAG_FAIL]\ndebounce_ms = 70000\n", &g_cfg);
    TEST_ASSERT_EQUAL_UINT16(CHANNEL_TABLE[CH_L_MAG_FAIL].debounce_ms,
                             g_cfg.ch[CH_L_MAG_FAIL].debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(1, w);
}

void test_debounce_non_numeric_rejected(void) {
    uint8_t w = alarm_cfg_parse("[L_MAG_FAIL]\ndebounce_ms = fast\n", &g_cfg);
    TEST_ASSERT_EQUAL_UINT16(CHANNEL_TABLE[CH_L_MAG_FAIL].debounce_ms,
                             g_cfg.ch[CH_L_MAG_FAIL].debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(1, w);
}

void test_crlf_line_endings_parse(void) {
    uint8_t w = alarm_cfg_parse("[FUEL_PRESS_LOW]\r\nactive_high = no\r\npull = up\r\n", &g_cfg);
    TEST_ASSERT_FALSE(g_cfg.ch[CH_FUEL_PRESS_LOW].active_high);
    TEST_ASSERT_EQUAL_UINT8(0, w);
}

// --- Parsing: warnings --------------------------------------------------------

void test_clean_file_has_no_warnings(void) {
    uint8_t w = alarm_cfg_parse("; comment\n[defaults]\nactive_high = yes\npull = down\n"
                                "[FUEL_PRESS_LOW]\nactive_high = no\npull = up\n", &g_cfg);
    TEST_ASSERT_EQUAL_UINT8(0, w);
}

void test_misspelled_section_warns_and_changes_nothing(void) {
    uint8_t w = alarm_cfg_parse("[OIL_PRES_LOW]\nactive_high = no\n", &g_cfg);
    TEST_ASSERT_TRUE(g_cfg.ch[CH_OIL_PRESS_LOW].active_high);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_unknown_key_warns(void) {
    uint8_t w = alarm_cfg_parse("[CO_DETECT]\nactive_hgh = no\n", &g_cfg);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_bad_bool_value_warns(void) {
    uint8_t w = alarm_cfg_parse("[CO_DETECT]\nactive_high = maybe\n", &g_cfg);
    TEST_ASSERT_TRUE(g_cfg.ch[CH_CO_DETECT].active_high);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_bad_pull_value_warns(void) {
    uint8_t w = alarm_cfg_parse("[CO_DETECT]\npull = sideways\n", &g_cfg);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_active_high_with_pull_up_warns(void) {
    // An open wire would read as permanently asserted.
    uint8_t w = alarm_cfg_parse("[CO_DETECT]\nactive_high = yes\npull = up\n", &g_cfg);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_active_low_with_pull_down_warns(void) {
    uint8_t w = alarm_cfg_parse("[CO_DETECT]\nactive_high = no\npull = down\n", &g_cfg);
    TEST_ASSERT_TRUE(w >= 1);
}

void test_defaults_section_is_order_independent(void) {
    alarm_cfg_parse("[CO_DETECT]\ndebounce_ms = 100\n[defaults]\ndebounce_ms = 1000\n", &g_cfg);
    TEST_ASSERT_EQUAL_UINT16(100,  g_cfg.ch[CH_CO_DETECT].debounce_ms);
    TEST_ASSERT_EQUAL_UINT16(1000, g_cfg.ch[CH_L_MAG_FAIL].debounce_ms);
}

// --- Loading ------------------------------------------------------------------

void test_load_not_found_is_no_file(void) {
    hal_mock_set_status(HAL_SD_NOT_FOUND);
    TEST_ASSERT_EQUAL(ALARM_CFG_NO_FILE, alarm_cfg_load(&g_cfg));
}

void test_load_too_big_is_error(void) {
    hal_mock_set_status(HAL_SD_TOO_BIG);
    TEST_ASSERT_EQUAL(ALARM_CFG_ERROR, alarm_cfg_load(&g_cfg));
    TEST_ASSERT_TRUE(g_cfg.ch[CH_CO_DETECT].active_high);   // fallback applied
}

void test_load_io_error_is_error(void) {
    hal_mock_set_status(HAL_SD_IO_ERROR);
    TEST_ASSERT_EQUAL(ALARM_CFG_ERROR, alarm_cfg_load(&g_cfg));
}

void test_load_with_warnings_is_warn(void) {
    hal_mock_set_file("[OIL_PRES_LOW]\nactive_high = no\n");
    TEST_ASSERT_EQUAL(ALARM_CFG_WARN, alarm_cfg_load(&g_cfg));
}

void test_load_clean_is_ok(void) {
    hal_mock_set_file("[FUEL_PRESS_LOW]\nactive_high = no\npull = up\n");
    TEST_ASSERT_EQUAL(ALARM_CFG_OK, alarm_cfg_load(&g_cfg));
    TEST_ASSERT_FALSE(g_cfg.ch[CH_FUEL_PRESS_LOW].active_high);
}

void test_load_6kb_commented_file_is_ok(void) {
    static char big[6200];
    size_t n = 0;
    while (n + 64 < 6000) {
        memcpy(big + n, "; ------------------------------ padding comment -----------\n", 61);
        n += 61;
    }
    strcpy(big + n, "[FUEL_PRESS_LOW]\nactive_high = no\npull = up\n");
    hal_mock_set_file(big);
    TEST_ASSERT_EQUAL(ALARM_CFG_OK, alarm_cfg_load(&g_cfg));
    TEST_ASSERT_FALSE(g_cfg.ch[CH_FUEL_PRESS_LOW].active_high);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inline_comment_stripped_from_value);
    RUN_TEST(test_debounce_overflow_rejected_not_wrapped);
    RUN_TEST(test_debounce_non_numeric_rejected);
    RUN_TEST(test_crlf_line_endings_parse);
    RUN_TEST(test_clean_file_has_no_warnings);
    RUN_TEST(test_misspelled_section_warns_and_changes_nothing);
    RUN_TEST(test_unknown_key_warns);
    RUN_TEST(test_bad_bool_value_warns);
    RUN_TEST(test_bad_pull_value_warns);
    RUN_TEST(test_active_high_with_pull_up_warns);
    RUN_TEST(test_active_low_with_pull_down_warns);
    RUN_TEST(test_defaults_section_is_order_independent);
    RUN_TEST(test_load_not_found_is_no_file);
    RUN_TEST(test_load_too_big_is_error);
    RUN_TEST(test_load_io_error_is_error);
    RUN_TEST(test_load_with_warnings_is_warn);
    RUN_TEST(test_load_clean_is_ok);
    RUN_TEST(test_load_6kb_commented_file_is_ok);
    return UNITY_END();
}
