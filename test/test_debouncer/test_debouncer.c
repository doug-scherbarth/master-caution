// test/test_debouncer/test_debouncer.c
//
// Unity tests for the debouncer.
// Run on the host: pio test -e host -f test_debouncer

#include <unity.h>
#include <string.h>
#include "debouncer.h"
#include "channel_table.h"

static bool raw[CHANNEL_COUNT];
static bool out[CHANNEL_COUNT];

void setUp(void) {
    memset(raw, 0, sizeof(raw));
    memset(out, 0, sizeof(out));
    debouncer_init();
}

void tearDown(void) {}

// --- Basics -----------------------------------------------------------

void test_initial_state_all_false(void) {
    debouncer_tick(0, raw, out);
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        TEST_ASSERT_FALSE_MESSAGE(out[i], CHANNEL_TABLE[i].name);
    }
}

void test_qualified_assertion_propagates_at_default_500ms(void) {
    raw[CH_CO_DETECT] = true;
    debouncer_tick(0, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    debouncer_tick(499, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    debouncer_tick(500, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);
}

void test_qualified_clear_propagates(void) {
    raw[CH_CO_DETECT] = true;
    debouncer_tick(0, raw, out);
    debouncer_tick(500, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);

    raw[CH_CO_DETECT] = false;
    debouncer_tick(600, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);   // qualifying

    debouncer_tick(1099, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);

    debouncer_tick(1100, raw, out);        // 600 + 500
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);
}

// --- Glitch & chatter rejection --------------------------------------

void test_quick_glitch_does_not_propagate(void) {
    raw[CH_CO_DETECT] = true;
    debouncer_tick(0, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    raw[CH_CO_DETECT] = false;             // glitch ends 100ms in
    debouncer_tick(100, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    debouncer_tick(2000, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);
}

void test_chatter_resets_qualification_timer(void) {
    raw[CH_CO_DETECT] = true;
    debouncer_tick(0, raw, out);
    debouncer_tick(400, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    // Glitch back to false — cancels transition
    raw[CH_CO_DETECT] = false;
    debouncer_tick(401, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    // Re-asserts; timer must restart from now (402), not from t=0
    raw[CH_CO_DETECT] = true;
    debouncer_tick(402, raw, out);
    debouncer_tick(901, raw, out);         // 402 + 500 - 1
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    debouncer_tick(902, raw, out);         // 402 + 500
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);
}

// --- Per-channel timing ----------------------------------------------

void test_per_channel_timing_flaps_qualifies_first(void) {
    // FLAPS_DEPLOYED debounce = 250ms; CO_DETECT = 500ms
    raw[CH_FLAPS_DEPLOYED] = true;
    raw[CH_CO_DETECT]      = true;

    debouncer_tick(0, raw, out);
    debouncer_tick(250, raw, out);
    TEST_ASSERT_TRUE (out[CH_FLAPS_DEPLOYED]);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    debouncer_tick(500, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);
}

// --- Channel independence -------------------------------------------

void test_channels_qualify_independently(void) {
    raw[CH_CO_DETECT]     = true;
    raw[CH_OIL_PRESS_LOW] = true;
    debouncer_tick(0, raw, out);
    debouncer_tick(500, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);
    TEST_ASSERT_TRUE(out[CH_OIL_PRESS_LOW]);

    // Clear one — other unaffected
    raw[CH_CO_DETECT] = false;
    debouncer_tick(1000, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);     // qualifying
    TEST_ASSERT_TRUE(out[CH_OIL_PRESS_LOW]); // stable

    debouncer_tick(1500, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);
    TEST_ASSERT_TRUE (out[CH_OIL_PRESS_LOW]);
}

// --- millis() wrap -----------------------------------------------------

void test_qualification_survives_millis_wrap(void) {
    // Start the qualification 100ms before uint32_t wraps.
    const uint32_t near_wrap = 0xFFFFFFFFu - 100u;

    raw[CH_CO_DETECT] = true;
    debouncer_tick(near_wrap, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    // 499ms after start — still under qualification (now_ms has wrapped past 0)
    debouncer_tick(near_wrap + 499u, raw, out);
    TEST_ASSERT_FALSE(out[CH_CO_DETECT]);

    // 500ms after start — modular subtraction (now - started) yields 500 cleanly
    debouncer_tick(near_wrap + 500u, raw, out);
    TEST_ASSERT_TRUE(out[CH_CO_DETECT]);
}

// --- Test runner ------------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_all_false);
    RUN_TEST(test_qualified_assertion_propagates_at_default_500ms);
    RUN_TEST(test_qualified_clear_propagates);
    RUN_TEST(test_quick_glitch_does_not_propagate);
    RUN_TEST(test_chatter_resets_qualification_timer);
    RUN_TEST(test_per_channel_timing_flaps_qualifies_first);
    RUN_TEST(test_channels_qualify_independently);
    RUN_TEST(test_qualification_survives_millis_wrap);
    return UNITY_END();
}
