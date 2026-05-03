// test/test_audio_queue/test_audio_queue.c
//
// Unity tests for the audio queue.
// Run on the host: pio test -e host -f test_audio_queue

#include <unity.h>
#include "audio_queue.h"
#include "alarm_table.h"   // wav_id enum

// HAL mock instrumentation ------------------------------------------
extern int     hal_play_calls;
extern uint8_t hal_play_last_wav;
extern uint8_t hal_play_log[];
extern int     hal_play_log_count;
void hal_mock_reset(void);
void hal_mock_set_busy(bool busy);

void setUp(void) {
    audio_queue_init();
    hal_mock_reset();
}
void tearDown(void) {}

// --- Empty queue ---------------------------------------------------

void test_empty_queue_does_not_play(void) {
    audio_queue_tick(0);
    TEST_ASSERT_EQUAL(0, hal_play_calls);
}

// --- Basic dispatch -----------------------------------------------

void test_single_push_dispatches_on_tick(void) {
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_tick(0);
    TEST_ASSERT_EQUAL(1, hal_play_calls);
    TEST_ASSERT_EQUAL(WAV_CO, hal_play_last_wav);
}

void test_dispatch_does_not_repeat_while_busy(void) {
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_tick(0);
    audio_queue_tick(10);
    audio_queue_tick(20);
    TEST_ASSERT_EQUAL(1, hal_play_calls);
}

void test_next_dispatches_when_busy_clears(void) {
    audio_queue_push(WAV_CO,         SEV_HIGH);
    audio_queue_push(WAV_OIL_PRESS,  SEV_HIGH);
    audio_queue_tick(0);   // dispatches CO; busy=true
    audio_queue_tick(10);  // still busy, nothing happens
    TEST_ASSERT_EQUAL(1, hal_play_calls);

    hal_mock_set_busy(false);   // playback finished
    audio_queue_tick(20);
    TEST_ASSERT_EQUAL(2, hal_play_calls);
    TEST_ASSERT_EQUAL(WAV_OIL_PRESS, hal_play_last_wav);
}

// --- Severity ordering --------------------------------------------

void test_higher_severity_plays_before_lower_when_idle(void) {
    // Pushed in low-to-high order; dispatch should go high first.
    audio_queue_push(WAV_BOOST,     SEV_LOW);
    audio_queue_push(WAV_PRIM_ALT,  SEV_MED);
    audio_queue_push(WAV_CO,        SEV_HIGH);

    audio_queue_tick(0);
    TEST_ASSERT_EQUAL(WAV_CO, hal_play_log[0]);

    hal_mock_set_busy(false); audio_queue_tick(10);
    TEST_ASSERT_EQUAL(WAV_PRIM_ALT, hal_play_log[1]);

    hal_mock_set_busy(false); audio_queue_tick(20);
    TEST_ASSERT_EQUAL(WAV_BOOST, hal_play_log[2]);
}

void test_no_preemption_higher_pushed_during_playback_waits(void) {
    audio_queue_push(WAV_BOOST, SEV_LOW);
    audio_queue_tick(0);   // dispatches BOOST; busy
    TEST_ASSERT_EQUAL(WAV_BOOST, hal_play_last_wav);

    // Higher severity arrives mid-playback — must NOT preempt.
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_tick(10);
    TEST_ASSERT_EQUAL(1, hal_play_calls);
    TEST_ASSERT_EQUAL(WAV_BOOST, hal_play_last_wav);

    hal_mock_set_busy(false);
    audio_queue_tick(20);
    TEST_ASSERT_EQUAL(2, hal_play_calls);
    TEST_ASSERT_EQUAL(WAV_CO, hal_play_last_wav);
}

void test_fifo_within_same_severity(void) {
    audio_queue_push(WAV_CO,        SEV_HIGH);
    audio_queue_push(WAV_OIL_PRESS, SEV_HIGH);
    audio_queue_push(WAV_CHT,       SEV_HIGH);

    audio_queue_tick(0);
    hal_mock_set_busy(false); audio_queue_tick(10);
    hal_mock_set_busy(false); audio_queue_tick(20);

    TEST_ASSERT_EQUAL(WAV_CO,        hal_play_log[0]);
    TEST_ASSERT_EQUAL(WAV_OIL_PRESS, hal_play_log[1]);
    TEST_ASSERT_EQUAL(WAV_CHT,       hal_play_log[2]);
}

void test_mixed_severities_drain_correctly(void) {
    audio_queue_push(WAV_BOOST,     SEV_LOW);    // 1st low
    audio_queue_push(WAV_CO,        SEV_HIGH);   // 1st high
    audio_queue_push(WAV_FLAPS_OS,  SEV_LOW);    // 2nd low
    audio_queue_push(WAV_OIL_PRESS, SEV_HIGH);   // 2nd high
    audio_queue_push(WAV_PRIM_ALT,  SEV_MED);    // 1st med

    // Expect: HIGH(CO), HIGH(OIL_PRESS), MED(PRIM_ALT), LOW(BOOST), LOW(FLAPS_OS)
    for (int i = 0; i < 5; i++) {
        audio_queue_tick(i * 10);
        hal_mock_set_busy(false);
    }

    TEST_ASSERT_EQUAL(WAV_CO,        hal_play_log[0]);
    TEST_ASSERT_EQUAL(WAV_OIL_PRESS, hal_play_log[1]);
    TEST_ASSERT_EQUAL(WAV_PRIM_ALT,  hal_play_log[2]);
    TEST_ASSERT_EQUAL(WAV_BOOST,     hal_play_log[3]);
    TEST_ASSERT_EQUAL(WAV_FLAPS_OS,  hal_play_log[4]);
}

// --- Duplicate suppression ----------------------------------------

void test_duplicate_in_queue_dropped(void) {
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_push(WAV_CO, SEV_HIGH);   // dup while still queued
    audio_queue_push(WAV_CO, SEV_HIGH);   // dup while still queued

    audio_queue_tick(0);
    hal_mock_set_busy(false);
    audio_queue_tick(10);

    TEST_ASSERT_EQUAL(1, hal_play_calls);
    TEST_ASSERT_EQUAL(2, audio_queue_dropped_dup_count());
}

void test_duplicate_during_playback_dropped(void) {
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_tick(0);   // CO playing now

    audio_queue_push(WAV_CO, SEV_HIGH);   // dup while playing
    TEST_ASSERT_EQUAL(1, audio_queue_dropped_dup_count());

    hal_mock_set_busy(false);
    audio_queue_tick(10);
    TEST_ASSERT_EQUAL(1, hal_play_calls);   // not replayed
}

void test_replay_allowed_after_completion(void) {
    audio_queue_push(WAV_CO, SEV_HIGH);
    audio_queue_tick(0);
    hal_mock_set_busy(false);
    audio_queue_tick(10);   // CO done playing, queue empty

    audio_queue_push(WAV_CO, SEV_HIGH);   // re-trigger after clear → not a dup
    audio_queue_tick(20);

    TEST_ASSERT_EQUAL(2, hal_play_calls);
    TEST_ASSERT_EQUAL(0, audio_queue_dropped_dup_count());
}

// --- Capacity / overflow ------------------------------------------

void test_full_queue_drops_with_counter(void) {
    // Capacity is 16 — fill to brim with distinct wav_ids.
    for (uint8_t i = 0; i < 16; i++) {
        audio_queue_push(i, SEV_HIGH);
    }
    TEST_ASSERT_EQUAL(0, audio_queue_dropped_full_count());

    // Two more should drop with counter.
    audio_queue_push(50, SEV_HIGH);
    audio_queue_push(51, SEV_HIGH);
    TEST_ASSERT_EQUAL(2, audio_queue_dropped_full_count());
}

// --- Test runner --------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_queue_does_not_play);
    RUN_TEST(test_single_push_dispatches_on_tick);
    RUN_TEST(test_dispatch_does_not_repeat_while_busy);
    RUN_TEST(test_next_dispatches_when_busy_clears);
    RUN_TEST(test_higher_severity_plays_before_lower_when_idle);
    RUN_TEST(test_no_preemption_higher_pushed_during_playback_waits);
    RUN_TEST(test_fifo_within_same_severity);
    RUN_TEST(test_mixed_severities_drain_correctly);
    RUN_TEST(test_duplicate_in_queue_dropped);
    RUN_TEST(test_duplicate_during_playback_dropped);
    RUN_TEST(test_replay_allowed_after_completion);
    RUN_TEST(test_full_queue_drops_with_counter);
    return UNITY_END();
}
