// test/test_regress_startup/test_regress_startup.c
// Regression for review items 2 (unit level) and 5.
//
// Contract (TASKS.md, Tasks 4 and 6):
//   - Channels normally asserted with the engine stopped do not raise CH_FAULT.
//     channel_descriptor_t gains `bool expected_at_rest`; set for OIL_PRESS_LOW,
//     FUEL_PRESS_LOW, PRIM_ALT_FAIL, SEC_ALT_FAIL (mags: see Task 6 note).
//   - Knob position never raises DIMMER_WARN. The check becomes a BUS_SENSE
//     plausibility check: warn only if BUS_SENSE < 10 V equivalent (2532 counts).
//   - SS_ACK_WAIT times out after STARTUP_ACK_TIMEOUT_MS (10 s) -> DONE.

#include <unity.h>
#include "startup.h"
#include "channel_table.h"
#include "hal.h"

extern uint16_t hal_led_duty[4];
extern int      hal_play_calls;
void hal_mock_reset(void);
void hal_mock_set_busy(bool busy);
void hal_mock_set_alarm(uint8_t ch, bool val);
void hal_mock_set_adc(hal_adc_ch_t ch, uint16_t val);

#define RED   hal_led_duty[HAL_LED_RED]
#define GREEN hal_led_duty[HAL_LED_GREEN]
#define BLUE  hal_led_duty[HAL_LED_BLUE]

#define BUS_12V   3039u
#define WHITE_END 1900u     // 3 x 500 ms colour steps + 400 ms white
#define ACK_TIMEOUT_MS 10000u

// Run the colour sweep to the decision point at the end of the white step.
static void sweep(void) {
    startup_init(0, false, false);
    startup_tick(500); startup_tick(1000); startup_tick(1500); startup_tick(WHITE_END);
}

static void assert_went_straight_to_tones(void) {
    TEST_ASSERT_EQUAL_INT(1, hal_play_calls);   // TONE_LO started
    TEST_ASSERT_EQUAL_UINT16(0, BLUE);
    TEST_ASSERT_EQUAL_UINT16(0, RED);
}

void setUp(void) {
    hal_mock_reset();
    hal_mock_set_adc(HAL_ADC_BUS_SENSE, BUS_12V);
    hal_mock_set_adc(HAL_ADC_DIM_IN,    1500u);
}
void tearDown(void) {}

// --- CH_FAULT: expected-at-rest channels ------------------------------------

void test_fuel_press_low_at_rest_no_ch_fault(void) {
    hal_mock_set_alarm(CH_FUEL_PRESS_LOW, true);
    sweep();
    assert_went_straight_to_tones();
}

void test_alternator_fail_at_rest_no_ch_fault(void) {
    hal_mock_set_alarm(CH_PRIM_ALT_FAIL, true);
    hal_mock_set_alarm(CH_SEC_ALT_FAIL,  true);
    sweep();
    assert_went_straight_to_tones();
}

void test_engine_off_combination_no_ch_fault(void) {
    hal_mock_set_alarm(CH_OIL_PRESS_LOW,  true);
    hal_mock_set_alarm(CH_FUEL_PRESS_LOW, true);
    hal_mock_set_alarm(CH_PRIM_ALT_FAIL,  true);
    hal_mock_set_alarm(CH_SEC_ALT_FAIL,   true);
    sweep();
    assert_went_straight_to_tones();
}

void test_co_asserted_at_rest_still_ch_fault(void) {       // guard
    hal_mock_set_alarm(CH_CO_DETECT, true);
    sweep();
    TEST_ASSERT_EQUAL_UINT16(4095, BLUE);
    TEST_ASSERT_EQUAL_INT(0, hal_play_calls);
}

void test_expected_at_rest_flags(void) {
    TEST_ASSERT_TRUE (CHANNEL_TABLE[CH_OIL_PRESS_LOW].expected_at_rest);
    TEST_ASSERT_TRUE (CHANNEL_TABLE[CH_FUEL_PRESS_LOW].expected_at_rest);
    TEST_ASSERT_TRUE (CHANNEL_TABLE[CH_PRIM_ALT_FAIL].expected_at_rest);
    TEST_ASSERT_TRUE (CHANNEL_TABLE[CH_SEC_ALT_FAIL].expected_at_rest);
    TEST_ASSERT_FALSE(CHANNEL_TABLE[CH_CO_DETECT].expected_at_rest);
    TEST_ASSERT_FALSE(CHANNEL_TABLE[CH_CHT_OVERTEMP].expected_at_rest);
}

// --- DIMMER_WARN: knob position never warns ----------------------------------

void test_knob_at_zero_no_dimmer_warn(void) {
    hal_mock_set_adc(HAL_ADC_DIM_IN, 0u);
    sweep();
    assert_went_straight_to_tones();
}

void test_knob_at_full_with_diode_or_offset_no_dimmer_warn(void) {
    // Pot bus sits ~0.4 V above BUS_SENSE: ratio pegs slightly above 1.0.
    hal_mock_set_adc(HAL_ADC_DIM_IN, 3140u);
    sweep();
    assert_went_straight_to_tones();
}

void test_bus_sense_implausibly_low_warns(void) {
    hal_mock_set_adc(HAL_ADC_BUS_SENSE, 1000u);   // ~4 V: divider fault
    hal_mock_set_adc(HAL_ADC_DIM_IN,     500u);
    sweep();
    TEST_ASSERT_EQUAL_UINT16(4095, RED);          // amber = R + G
    TEST_ASSERT_EQUAL_UINT16(4095, GREEN);
}

// --- ACK_WAIT timeout ---------------------------------------------------------

static uint32_t run_to_ack_wait(void) {
    sweep();                                      // TONE_LO playing
    hal_mock_set_busy(false); startup_tick(2000); // -> TONE_MID
    hal_mock_set_busy(false); startup_tick(2100); // -> TONE_HI
    hal_mock_set_busy(false); startup_tick(2200); // -> ACK_WAIT
    return 2200u;
}

void test_ack_wait_holds_before_timeout(void) {
    uint32_t t0 = run_to_ack_wait();
    startup_tick(t0 + ACK_TIMEOUT_MS - 1u);
    TEST_ASSERT_TRUE(startup_active());
}

void test_ack_wait_times_out_without_press(void) {
    uint32_t t0 = run_to_ack_wait();
    startup_tick(t0 + ACK_TIMEOUT_MS);
    TEST_ASSERT_FALSE(startup_active());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fuel_press_low_at_rest_no_ch_fault);
    RUN_TEST(test_alternator_fail_at_rest_no_ch_fault);
    RUN_TEST(test_engine_off_combination_no_ch_fault);
    RUN_TEST(test_co_asserted_at_rest_still_ch_fault);
    RUN_TEST(test_expected_at_rest_flags);
    RUN_TEST(test_knob_at_zero_no_dimmer_warn);
    RUN_TEST(test_knob_at_full_with_diode_or_offset_no_dimmer_warn);
    RUN_TEST(test_bus_sense_implausibly_low_warns);
    RUN_TEST(test_ack_wait_holds_before_timeout);
    RUN_TEST(test_ack_wait_times_out_without_press);
    return UNITY_END();
}
