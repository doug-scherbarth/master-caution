// test/test_startup/test_startup.c
// Unity tests for the power-up startup sequence.
// Run on the host: pio test -e host -f test_startup

#include <unity.h>
#include "startup.h"
#include "hal.h"

extern uint16_t hal_led_duty[4];
extern int      hal_play_calls;
extern uint8_t  hal_play_log[];
extern int      hal_play_log_count;
void hal_mock_reset(void);
void hal_mock_set_busy(bool busy);
void hal_mock_set_sd_ok(bool ok);

#define RED   hal_led_duty[HAL_LED_RED]
#define GREEN hal_led_duty[HAL_LED_GREEN]
#define BLUE  hal_led_duty[HAL_LED_BLUE]

#define LED_MS   500u
#define WHITE_MS 400u
#define FLASH_MS 250u

#define WAV_LO  13
#define WAV_MID 14
#define WAV_HI  15

void setUp(void) { hal_mock_reset(); startup_init(0); }
void tearDown(void) {}

// --- Active state --------------------------------------------------

void test_active_immediately_after_init(void) {
    TEST_ASSERT_TRUE(startup_active());
}

// --- Color sweep ---------------------------------------------------

void test_red_phase_on_init(void) {
    TEST_ASSERT_EQUAL(4095, RED);
    TEST_ASSERT_EQUAL(0,    GREEN);
    TEST_ASSERT_EQUAL(0,    BLUE);
}

void test_red_holds_before_timeout(void) {
    startup_tick(LED_MS - 1);
    TEST_ASSERT_EQUAL(4095, RED);
}

void test_red_to_green_at_timeout(void) {
    startup_tick(LED_MS);
    TEST_ASSERT_EQUAL(0,    RED);
    TEST_ASSERT_EQUAL(4095, GREEN);
    TEST_ASSERT_EQUAL(0,    BLUE);
}

void test_green_to_blue_at_timeout(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    TEST_ASSERT_EQUAL(0,    RED);
    TEST_ASSERT_EQUAL(0,    GREEN);
    TEST_ASSERT_EQUAL(4095, BLUE);
}

void test_blue_to_white_at_timeout(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    TEST_ASSERT_EQUAL(4095, RED);
    TEST_ASSERT_EQUAL(4095, GREEN);
    TEST_ASSERT_EQUAL(4095, BLUE);
}

// --- Tone chime ----------------------------------------------------

void test_leds_off_when_tones_start(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    startup_tick(LED_MS * 3 + WHITE_MS);   // → TONE_LO
    TEST_ASSERT_EQUAL(0, RED);
    TEST_ASSERT_EQUAL(0, GREEN);
    TEST_ASSERT_EQUAL(0, BLUE);
}

void test_tone_sequence_lo_mid_hi(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    startup_tick(LED_MS * 3 + WHITE_MS);       // → TONE_LO

    hal_mock_set_busy(false);
    startup_tick(LED_MS * 3 + WHITE_MS + 10);  // → TONE_MID

    hal_mock_set_busy(false);
    startup_tick(LED_MS * 3 + WHITE_MS + 20);  // → TONE_HI

    TEST_ASSERT_EQUAL(3,       hal_play_calls);
    TEST_ASSERT_EQUAL(WAV_LO,  hal_play_log[0]);
    TEST_ASSERT_EQUAL(WAV_MID, hal_play_log[1]);
    TEST_ASSERT_EQUAL(WAV_HI,  hal_play_log[2]);
}

void test_tone_lo_holds_while_busy(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    startup_tick(LED_MS * 3 + WHITE_MS);   // → TONE_LO; busy=true
    int calls = hal_play_calls;
    startup_tick(LED_MS * 3 + WHITE_MS + 100);
    TEST_ASSERT_EQUAL(calls, hal_play_calls);
}

// --- ACK wait phase ------------------------------------------------

static void advance_to_ack_wait(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    startup_tick(LED_MS * 3 + WHITE_MS);
    hal_mock_set_busy(false); startup_tick(LED_MS * 3 + WHITE_MS + 10);
    hal_mock_set_busy(false); startup_tick(LED_MS * 3 + WHITE_MS + 20);
    hal_mock_set_busy(false); startup_tick(LED_MS * 3 + WHITE_MS + 30);
}

void test_green_on_entering_ack_wait(void) {
    advance_to_ack_wait();
    TEST_ASSERT_EQUAL(4095, GREEN);
    TEST_ASSERT_EQUAL(0,    RED);
    TEST_ASSERT_EQUAL(0,    BLUE);
}

void test_still_active_in_ack_wait(void) {
    advance_to_ack_wait();
    TEST_ASSERT_TRUE(startup_active());
}

void test_green_flashes_off_after_250ms(void) {
    advance_to_ack_wait();
    uint32_t t = LED_MS * 3 + WHITE_MS + 30;
    startup_tick(t + FLASH_MS);
    TEST_ASSERT_EQUAL(0, GREEN);
}

void test_green_flashes_on_after_500ms(void) {
    advance_to_ack_wait();
    uint32_t t = LED_MS * 3 + WHITE_MS + 30;
    startup_tick(t + FLASH_MS);
    startup_tick(t + FLASH_MS * 2);
    TEST_ASSERT_EQUAL(4095, GREEN);
}

void test_button_press_before_ack_wait_ignored(void) {
    // Press during RED phase — must not complete startup.
    startup_on_button_press(100);
    TEST_ASSERT_TRUE(startup_active());
}

void test_button_press_in_ack_wait_completes_startup(void) {
    advance_to_ack_wait();
    startup_on_button_press(9999);
    TEST_ASSERT_FALSE(startup_active());
}

void test_leds_off_after_ack(void) {
    advance_to_ack_wait();
    startup_on_button_press(9999);
    TEST_ASSERT_EQUAL(0, RED);
    TEST_ASSERT_EQUAL(0, GREEN);
    TEST_ASSERT_EQUAL(0, BLUE);
}

// --- SD error path -------------------------------------------------

static void advance_to_after_white(void) {
    startup_tick(LED_MS);
    startup_tick(LED_MS * 2);
    startup_tick(LED_MS * 3);
    startup_tick(LED_MS * 3 + WHITE_MS);
}

void test_sd_error_skips_tones_and_fast_flashes_red(void) {
    hal_mock_set_sd_ok(false);
    startup_init(0);
    advance_to_after_white();
    // Should be in SD_ERROR: no tones played, RED flashing
    TEST_ASSERT_EQUAL(0, hal_play_calls);
    TEST_ASSERT_EQUAL(4095, RED);
}

void test_sd_error_red_toggles_at_5hz(void) {
    hal_mock_set_sd_ok(false);
    startup_init(0);
    advance_to_after_white();
    uint32_t t = LED_MS * 3 + WHITE_MS;
    startup_tick(t + 100);   // one 100ms half-period → off
    TEST_ASSERT_EQUAL(0, RED);
    startup_tick(t + 200);   // another → on
    TEST_ASSERT_EQUAL(4095, RED);
}

void test_sd_error_transitions_to_ack_wait_after_5s(void) {
    hal_mock_set_sd_ok(false);
    startup_init(0);
    advance_to_after_white();
    uint32_t t = LED_MS * 3 + WHITE_MS;
    startup_tick(t + 4999);  // just before 5s — still error
    TEST_ASSERT_EQUAL(0, hal_play_calls);
    startup_tick(t + 5000);  // 5s elapsed → ACK_WAIT (green flash)
    TEST_ASSERT_EQUAL(4095, GREEN);
    TEST_ASSERT_EQUAL(0,    RED);
}

void test_sd_error_ack_completes_startup(void) {
    hal_mock_set_sd_ok(false);
    startup_init(0);
    advance_to_after_white();
    uint32_t t = LED_MS * 3 + WHITE_MS;
    startup_tick(t + 5000);  // → ACK_WAIT
    startup_on_button_press(t + 5001);
    TEST_ASSERT_FALSE(startup_active());
}

// --- Test runner ---------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_active_immediately_after_init);
    RUN_TEST(test_red_phase_on_init);
    RUN_TEST(test_red_holds_before_timeout);
    RUN_TEST(test_red_to_green_at_timeout);
    RUN_TEST(test_green_to_blue_at_timeout);
    RUN_TEST(test_blue_to_white_at_timeout);
    RUN_TEST(test_leds_off_when_tones_start);
    RUN_TEST(test_tone_sequence_lo_mid_hi);
    RUN_TEST(test_tone_lo_holds_while_busy);
    RUN_TEST(test_green_on_entering_ack_wait);
    RUN_TEST(test_still_active_in_ack_wait);
    RUN_TEST(test_green_flashes_off_after_250ms);
    RUN_TEST(test_green_flashes_on_after_500ms);
    RUN_TEST(test_button_press_before_ack_wait_ignored);
    RUN_TEST(test_button_press_in_ack_wait_completes_startup);
    RUN_TEST(test_leds_off_after_ack);
    RUN_TEST(test_sd_error_skips_tones_and_fast_flashes_red);
    RUN_TEST(test_sd_error_red_toggles_at_5hz);
    RUN_TEST(test_sd_error_transitions_to_ack_wait_after_5s);
    RUN_TEST(test_sd_error_ack_completes_startup);
    return UNITY_END();
}
