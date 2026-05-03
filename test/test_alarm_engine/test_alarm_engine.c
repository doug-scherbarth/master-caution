// test/test_alarm_engine/test_alarm_engine.c
//
// Unity tests for the alarm engine state machine.
// Run on the host: pio test -e host -f test_alarm_engine

#include <unity.h>
#include <string.h>
#include "alarm_engine.h"
#include "alarm_table.h"
#include "channel_table.h"

// Stub instrumentation (defined in audio_queue_stub.c)
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

// Lookup an alarm index by name so tests read naturally.
static uint8_t find_alarm(const char *name) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (strcmp(ALARM_TABLE[i].name, name) == 0) return i;
    }
    TEST_FAIL_MESSAGE("alarm not found in table");
    return 0xFF;
}

// --- Initial state -------------------------------------------------------

void test_initial_state_all_inactive(void) {
    TEST_ASSERT_EQUAL(SEV_NONE, alarm_engine_max_active_severity());
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        TEST_ASSERT_EQUAL(AS_INACTIVE, alarm_engine_runtime(i)->state);
    }
}

// --- Direct alarm: assert / hold / clear without ack --------------------

void test_direct_assertion_transitions_to_pending_and_fires_audio(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);

    uint8_t i = find_alarm("CO_DETECT");
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(i)->state);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);
    TEST_ASSERT_EQUAL(WAV_CO, audio_queue_last_wav_id);
    TEST_ASSERT_EQUAL(SEV_HIGH, audio_queue_last_severity);
    TEST_ASSERT_EQUAL(SEV_HIGH, alarm_engine_max_active_severity());
    TEST_ASSERT_TRUE(alarm_engine_any_pending_ack());
}

void test_held_assertion_does_not_re_fire_audio(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_tick(200, ch);
    alarm_engine_tick(300, ch);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);
}

void test_pending_clears_without_ack(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);

    uint8_t i = find_alarm("CO_DETECT");
    TEST_ASSERT_EQUAL(AS_INACTIVE, alarm_engine_runtime(i)->state);
    TEST_ASSERT_EQUAL(SEV_NONE, alarm_engine_max_active_severity());
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
}

// --- Acknowledge --------------------------------------------------------

void test_button_press_promotes_pending_to_acknowledged(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);

    uint8_t i = find_alarm("CO_DETECT");
    TEST_ASSERT_EQUAL(AS_ACKNOWLEDGED, alarm_engine_runtime(i)->state);
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
    TEST_ASSERT_EQUAL(SEV_HIGH, alarm_engine_max_active_severity());
}

void test_button_press_does_not_re_fire_audio(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);
}

void test_acknowledged_then_cleared_returns_to_inactive(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);

    uint8_t i = find_alarm("CO_DETECT");
    TEST_ASSERT_EQUAL(AS_INACTIVE, alarm_engine_runtime(i)->state);
    TEST_ASSERT_EQUAL(SEV_NONE, alarm_engine_max_active_severity());
}

void test_button_with_no_pending_is_noop(void) {
    alarm_engine_on_button_press(100);
    TEST_ASSERT_EQUAL(SEV_NONE, alarm_engine_max_active_severity());
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
}

void test_button_only_promotes_pending_not_already_acknowledged(void) {
    // CO asserts + ack
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);

    // OIL_PRESS asserts (now a fresh PENDING)
    ch[CH_OIL_PRESS_LOW] = true;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_TRUE(alarm_engine_any_pending_ack());

    // Press again — both should now be ACKNOWLEDGED
    alarm_engine_on_button_press(250);
    TEST_ASSERT_EQUAL(AS_ACKNOWLEDGED, alarm_engine_runtime(find_alarm("CO_DETECT"))->state);
    TEST_ASSERT_EQUAL(AS_ACKNOWLEDGED, alarm_engine_runtime(find_alarm("OIL_PRESS_LOW"))->state);
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
}

// --- Re-trigger (spec §4.6) --------------------------------------------

void test_retrigger_after_ack_clear_fires_audio_again(void) {
    // assert → ack → clear → re-assert
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);
    ch[CH_CO_DETECT] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);  // still just the original

    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(300, ch);

    uint8_t i = find_alarm("CO_DETECT");
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(i)->state);
    TEST_ASSERT_EQUAL(2, audio_queue_calls);
    TEST_ASSERT_TRUE(alarm_engine_any_pending_ack());
}

// --- Composite alarm: FLAPS_DEPLOYED && AIRSPEED_OVER_VFE ---------------

void test_composite_one_input_alone_does_not_fire(void) {
    ch[CH_FLAPS_DEPLOYED] = true;
    alarm_engine_tick(100, ch);
    TEST_ASSERT_EQUAL(AS_INACTIVE,
        alarm_engine_runtime(find_alarm("FLAPS_OVERSPEED"))->state);
    TEST_ASSERT_EQUAL(0, audio_queue_calls);

    ch[CH_FLAPS_DEPLOYED] = false;
    ch[CH_AIRSPEED_OVER_VFE] = true;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(AS_INACTIVE,
        alarm_engine_runtime(find_alarm("FLAPS_OVERSPEED"))->state);
    TEST_ASSERT_EQUAL(0, audio_queue_calls);
}

void test_composite_both_inputs_fires(void) {
    ch[CH_FLAPS_DEPLOYED]    = true;
    ch[CH_AIRSPEED_OVER_VFE] = true;
    alarm_engine_tick(100, ch);

    uint8_t i = find_alarm("FLAPS_OVERSPEED");
    TEST_ASSERT_EQUAL(AS_PENDING_ACK, alarm_engine_runtime(i)->state);
    TEST_ASSERT_EQUAL(WAV_FLAPS_OS, audio_queue_last_wav_id);
    TEST_ASSERT_EQUAL(SEV_LOW, audio_queue_last_severity);
}

void test_composite_clears_when_either_input_clears(void) {
    ch[CH_FLAPS_DEPLOYED]    = true;
    ch[CH_AIRSPEED_OVER_VFE] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);

    // Airspeed drops below Vfe — composite should clear
    ch[CH_AIRSPEED_OVER_VFE] = false;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(AS_INACTIVE,
        alarm_engine_runtime(find_alarm("FLAPS_OVERSPEED"))->state);
}

void test_composite_retriggers(void) {
    // Both true → pending → ack → one clears → both true again → new event
    ch[CH_FLAPS_DEPLOYED]    = true;
    ch[CH_AIRSPEED_OVER_VFE] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);
    TEST_ASSERT_EQUAL(1, audio_queue_calls);

    ch[CH_AIRSPEED_OVER_VFE] = false;
    alarm_engine_tick(200, ch);
    ch[CH_AIRSPEED_OVER_VFE] = true;
    alarm_engine_tick(300, ch);

    TEST_ASSERT_EQUAL(AS_PENDING_ACK,
        alarm_engine_runtime(find_alarm("FLAPS_OVERSPEED"))->state);
    TEST_ASSERT_EQUAL(2, audio_queue_calls);
}

// --- Severity rollup ---------------------------------------------------

void test_max_severity_tracks_highest_active(void) {
    ch[CH_BOOST_PUMP_ON] = true;
    alarm_engine_tick(100, ch);
    TEST_ASSERT_EQUAL(SEV_LOW, alarm_engine_max_active_severity());

    ch[CH_PRIM_ALT_FAIL] = true;
    alarm_engine_tick(200, ch);
    TEST_ASSERT_EQUAL(SEV_MED, alarm_engine_max_active_severity());

    ch[CH_OIL_PRESS_LOW] = true;
    alarm_engine_tick(300, ch);
    TEST_ASSERT_EQUAL(SEV_HIGH, alarm_engine_max_active_severity());

    // High clears — MED still active, severity steps down
    ch[CH_OIL_PRESS_LOW] = false;
    alarm_engine_tick(400, ch);
    TEST_ASSERT_EQUAL(SEV_MED, alarm_engine_max_active_severity());
}

void test_acknowledged_alarms_still_count_toward_severity(void) {
    ch[CH_CO_DETECT] = true;
    alarm_engine_tick(100, ch);
    alarm_engine_on_button_press(150);
    TEST_ASSERT_EQUAL(SEV_HIGH, alarm_engine_max_active_severity());
    TEST_ASSERT_FALSE(alarm_engine_any_pending_ack());
}

// --- Test runner -------------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_all_inactive);
    RUN_TEST(test_direct_assertion_transitions_to_pending_and_fires_audio);
    RUN_TEST(test_held_assertion_does_not_re_fire_audio);
    RUN_TEST(test_pending_clears_without_ack);
    RUN_TEST(test_button_press_promotes_pending_to_acknowledged);
    RUN_TEST(test_button_press_does_not_re_fire_audio);
    RUN_TEST(test_acknowledged_then_cleared_returns_to_inactive);
    RUN_TEST(test_button_with_no_pending_is_noop);
    RUN_TEST(test_button_only_promotes_pending_not_already_acknowledged);
    RUN_TEST(test_retrigger_after_ack_clear_fires_audio_again);
    RUN_TEST(test_composite_one_input_alone_does_not_fire);
    RUN_TEST(test_composite_both_inputs_fires);
    RUN_TEST(test_composite_clears_when_either_input_clears);
    RUN_TEST(test_composite_retriggers);
    RUN_TEST(test_max_severity_tracks_highest_active);
    RUN_TEST(test_acknowledged_alarms_still_count_toward_severity);
    return UNITY_END();
}
