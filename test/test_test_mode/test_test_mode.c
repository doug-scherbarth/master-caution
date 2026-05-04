// test/test_test_mode/test_test_mode.c
// Unity tests for test_mode state machine.
// Run on the host: pio test -e host -f test_test_mode

#include <unity.h>
#include "test_mode.h"
#include "hal.h"

// HAL mock instrumentation ------------------------------------------
extern uint16_t hal_led_duty[4];
extern int      hal_play_calls;
extern uint8_t  hal_play_log[];
extern int      hal_play_log_count;
void hal_mock_reset(void);
void hal_mock_set_busy(bool busy);

#define RED   hal_led_duty[HAL_LED_RED]
#define GREEN hal_led_duty[HAL_LED_GREEN]
#define BLUE  hal_led_duty[HAL_LED_BLUE]

#define LED_STEP 500u

void setUp(void) {
    test_mode_init();
    hal_mock_reset();
}
void tearDown(void) {}

// --- Idle state ----------------------------------------------------

void test_inactive_at_init(void) {
    TEST_ASSERT_FALSE(test_mode_active());
}

void test_trigger_activates(void) {
    test_mode_trigger(0);
    TEST_ASSERT_TRUE(test_mode_active());
}

void test_trigger_noop_when_already_active(void) {
    test_mode_trigger(0);
    // RED phase: r=4095
    uint16_t r_after_first = RED;
    // Force a tick to advance some time, then retrigger — state must not reset.
    test_mode_tick(100);
    test_mode_trigger(100);   // no-op
    TEST_ASSERT_EQUAL(r_after_first, RED);
    TEST_ASSERT_TRUE(test_mode_active());
}

// --- LED phases ----------------------------------------------------

void test_trigger_sets_red(void) {
    test_mode_trigger(0);
    TEST_ASSERT_EQUAL(4095, RED);
    TEST_ASSERT_EQUAL(0,    GREEN);
    TEST_ASSERT_EQUAL(0,    BLUE);
}

void test_red_holds_before_timeout(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP - 1);
    TEST_ASSERT_EQUAL(4095, RED);
    TEST_ASSERT_EQUAL(0,    GREEN);
}

void test_red_to_green_at_timeout(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    TEST_ASSERT_EQUAL(0,    RED);
    TEST_ASSERT_EQUAL(4095, GREEN);
    TEST_ASSERT_EQUAL(0,    BLUE);
}

void test_green_to_blue_at_timeout(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);       // → GREEN
    test_mode_tick(LED_STEP * 2);   // → BLUE
    TEST_ASSERT_EQUAL(0,    RED);
    TEST_ASSERT_EQUAL(0,    GREEN);
    TEST_ASSERT_EQUAL(4095, BLUE);
}

// --- Tone phases ---------------------------------------------------

void test_blue_to_tone_lo_at_timeout(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);   // → TONE_LO
    TEST_ASSERT_EQUAL(0, RED);
    TEST_ASSERT_EQUAL(0, GREEN);
    TEST_ASSERT_EQUAL(0, BLUE);
    TEST_ASSERT_EQUAL(1, hal_play_calls);
    TEST_ASSERT_EQUAL(TEST_WAV_LO, hal_play_log[0]);
}

void test_tone_lo_holds_while_busy(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);   // → TONE_LO; mock sets busy=true
    int calls_after_lo = hal_play_calls;

    test_mode_tick(LED_STEP * 3 + 100);  // still busy
    TEST_ASSERT_EQUAL(calls_after_lo, hal_play_calls);
}

void test_tone_sequence_lo_mid_hi(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);   // → TONE_LO

    hal_mock_set_busy(false);
    test_mode_tick(LED_STEP * 3 + 10);  // → TONE_MID

    hal_mock_set_busy(false);
    test_mode_tick(LED_STEP * 3 + 20);  // → TONE_HI

    TEST_ASSERT_EQUAL(3, hal_play_calls);
    TEST_ASSERT_EQUAL(TEST_WAV_LO,  hal_play_log[0]);
    TEST_ASSERT_EQUAL(TEST_WAV_MID, hal_play_log[1]);
    TEST_ASSERT_EQUAL(TEST_WAV_HI,  hal_play_log[2]);
}

void test_returns_idle_after_tone_hi(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);

    hal_mock_set_busy(false);
    test_mode_tick(LED_STEP * 3 + 10);

    hal_mock_set_busy(false);
    test_mode_tick(LED_STEP * 3 + 20);

    hal_mock_set_busy(false);
    test_mode_tick(LED_STEP * 3 + 30);  // → IDLE

    TEST_ASSERT_FALSE(test_mode_active());
}

void test_leds_off_after_sequence(void) {
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 10);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 20);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 30);

    TEST_ASSERT_EQUAL(0, RED);
    TEST_ASSERT_EQUAL(0, GREEN);
    TEST_ASSERT_EQUAL(0, BLUE);
}

void test_can_retrigger_after_completion(void) {
    // Run full sequence.
    test_mode_trigger(0);
    test_mode_tick(LED_STEP);
    test_mode_tick(LED_STEP * 2);
    test_mode_tick(LED_STEP * 3);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 10);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 20);
    hal_mock_set_busy(false); test_mode_tick(LED_STEP * 3 + 30);
    TEST_ASSERT_FALSE(test_mode_active());

    // Re-trigger a new sequence.
    hal_mock_reset();
    test_mode_trigger(5000);
    TEST_ASSERT_TRUE(test_mode_active());
    TEST_ASSERT_EQUAL(4095, RED);
}

// --- Test runner ---------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inactive_at_init);
    RUN_TEST(test_trigger_activates);
    RUN_TEST(test_trigger_noop_when_already_active);
    RUN_TEST(test_trigger_sets_red);
    RUN_TEST(test_red_holds_before_timeout);
    RUN_TEST(test_red_to_green_at_timeout);
    RUN_TEST(test_green_to_blue_at_timeout);
    RUN_TEST(test_blue_to_tone_lo_at_timeout);
    RUN_TEST(test_tone_lo_holds_while_busy);
    RUN_TEST(test_tone_sequence_lo_mid_hi);
    RUN_TEST(test_returns_idle_after_tone_hi);
    RUN_TEST(test_leds_off_after_sequence);
    RUN_TEST(test_can_retrigger_after_completion);
    return UNITY_END();
}
