// test/test_latch_reassert/test_latch_reassert.c
//
// Focused tests for the PENDING_ACK_CLEARED latch behaviour, specifically
// that a re-assertion from PENDING_ACK_CLEARED is silent (spec §4.2, §4.6).
//
// With v1.4.2 behaviour (audio fired on re-assert): tests 1, 2, 3, 4 fail.
// With v1.4.3 behaviour (re-assert is silent):      all 7 pass.

#include <unity.h>
#include <string.h>
#include "alarm_engine.h"
#include "alarm_table.h"
#include "channel_table.h"

extern int        audio_queue_calls;
extern uint8_t    audio_queue_last_wav_id;
extern severity_t audio_queue_last_severity;
void audio_queue_stub_reset(void);

static bool ch[CHANNEL_COUNT];

void setUp(void) {
    memset(ch, 0, sizeof(ch));
    audio_queue_stub_reset();
    alarm_engine_init();
}

void tearDown(void) {}

static uint8_t find_alarm(const char *name) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (strcmp(ALARM_TABLE[i].name, name) == 0) return i;
    }
    TEST_FAIL_MESSAGE("alarm not found in table");
    return 0xFF;
}

// --- Tests that FAIL with v1.4.2 (audio on re-assert) ---

// 1. Re-assertion from PENDING_ACK_CLEARED must not queue audio.
void test_reassert_from_cleared_fires_no_audio(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);

    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK_CLEARED,
                      alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(300, ch);

    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);  // still 1 — no new audio on re-assert
}

// 2. Multiple re-assertions before ack: audio count stays at 1.
void test_double_reassert_fires_no_extra_audio(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(300, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(400, ch);
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(500, ch);

    TEST_ASSERT_EQUAL(1, audio_queue_calls);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
}

// 3. After a chattering latch, ack + clear + re-assert is a fresh onset (1 more audio).
void test_retrigger_after_reassert_ack_announces_once_more(void) {
    // Phase 1: assert, clear (latch), re-assert (silent), ack → ACKNOWLEDGED
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(300, ch);
    alarm_engine_on_button_press(350);
    TEST_ASSERT_EQUAL(AS_ACKNOWLEDGED, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    // Phase 2: clear → INACTIVE, then fresh assert → new onset
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(400, ch);
    TEST_ASSERT_EQUAL(AS_INACTIVE, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(500, ch);

    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_EQUAL(2, audio_queue_calls);  // 1 initial + 1 retrigger; no extra from re-assert
}

// 4. A second alarm fires its own audio while the first is latched-cleared;
//    a re-assertion of the first must not add a third audio push.
void test_concurrent_alarm_onset_while_latch_cleared_is_independent(void) {
    // CO latches: assert then clear before ack
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);

    // OIL_PRESS fires (new onset — must announce)
    ch[CH_OIL_PRESS_LOW] = true;
    alarm_engine_tick(300, ch);
    TEST_ASSERT_EQUAL(2, audio_queue_calls);

    // CO re-asserts — must be silent
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(400, ch);

    TEST_ASSERT_EQUAL(2, audio_queue_calls);  // no third push
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("OIL_PRESS_LOW"))->state);
}

// --- Tests that PASS with either v1.4.2 or v1.4.3 behavior ---

// 5. Re-assertion returns alarm to PENDING_ACK state.
void test_reassert_from_cleared_state_is_pending_ack(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK_CLEARED,
                      alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(300, ch);

    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_TRUE(alarm_engine_any_pending_ack());
}

// 6. Button press on PENDING_ACK_CLEARED goes directly to INACTIVE.
void test_button_on_pending_cleared_goes_inactive(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK_CLEARED,
                      alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    alarm_engine_on_button_press(250);

    TEST_ASSERT_EQUAL(AS_INACTIVE, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
    TEST_ASSERT_EQUAL(SEV_NONE, alarm_engine_max_active_severity());
}

// 7. Button press on PENDING_ACK (condition still present) goes to ACKNOWLEDGED.
void test_button_on_pending_ack_goes_acknowledged(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);

    alarm_engine_on_button_press(150);

    TEST_ASSERT_EQUAL(AS_ACKNOWLEDGED, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
    TEST_ASSERT_EQUAL(SEV_HIGH, alarm_engine_max_active_severity());
}

// --- Test runner ---

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reassert_from_cleared_fires_no_audio);
    RUN_TEST(test_double_reassert_fires_no_extra_audio);
    RUN_TEST(test_retrigger_after_reassert_ack_announces_once_more);
    RUN_TEST(test_concurrent_alarm_onset_while_latch_cleared_is_independent);
    RUN_TEST(test_reassert_from_cleared_state_is_pending_ack);
    RUN_TEST(test_button_on_pending_cleared_goes_inactive);
    RUN_TEST(test_button_on_pending_ack_goes_acknowledged);
    return UNITY_END();
}
