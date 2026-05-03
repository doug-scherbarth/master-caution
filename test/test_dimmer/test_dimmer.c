// test/test_dimmer/test_dimmer.c
//
// Unity tests for the dimmer module.
// Run on the host: pio test -e host -f test_dimmer

#include <unity.h>
#include "dimmer.h"

// Helper: drive raw value at 50 Hz cadence for `duration_ms` starting at start_ms.
// Returns the elapsed end time (next tick should use this value or later).
static uint32_t drive(uint32_t start_ms, uint32_t duration_ms, uint16_t raw) {
    uint32_t t = start_ms;
    const uint32_t end = start_ms + duration_ms;
    while (t < end) {
        dimmer_tick(t, raw);
        t += 20;
    }
    return t;
}

void setUp(void) { dimmer_init(); }
void tearDown(void) {}

// --- Initial state ----------------------------------------------------

void test_initial_state_zero(void) {
    TEST_ASSERT_EQUAL(0, dimmer_get_norm_q12());
    TEST_ASSERT_FALSE(dimmer_failsafe_active());
}

// --- IIR convergence --------------------------------------------------

void test_converges_to_max_at_full_bright(void) {
    drive(0, 2000, 3700);
    TEST_ASSERT_EQUAL(4095, dimmer_get_norm_q12());
}

void test_converges_to_mid_at_mid_input(void) {
    drive(0, 2000, 1875);  // halfway between DIM_RAW_MIN and DIM_RAW_MAX
    TEST_ASSERT_INT_WITHIN(50, 2048, dimmer_get_norm_q12());
}

// --- Rate limiting ----------------------------------------------------

void test_rate_limited_to_50hz(void) {
    dimmer_tick(0, 3700);
    const uint16_t after_first = dimmer_get_norm_q12();

    // Within rate-limit window — no further sampling
    dimmer_tick(5,  3700);
    dimmer_tick(10, 3700);
    dimmer_tick(19, 3700);
    TEST_ASSERT_EQUAL(after_first, dimmer_get_norm_q12());

    // Crossing 20ms threshold — new sample taken
    dimmer_tick(20, 3700);
    TEST_ASSERT_GREATER_THAN(after_first, dimmer_get_norm_q12());
}

// --- Deadbands -------------------------------------------------------

void test_deadband_at_low_end_clamps_to_zero(void) {
    // raw just inside deadband (DIM_RAW_MIN=50, deadband=30 → up to 80)
    drive(0, 2000, 70);
    TEST_ASSERT_EQUAL(0, dimmer_get_norm_q12());
}

void test_deadband_at_high_end_clamps_to_max(void) {
    // raw just inside deadband (DIM_RAW_MAX=3700, deadband=30 → 3670+)
    drive(0, 2000, 3680);
    TEST_ASSERT_EQUAL(4095, dimmer_get_norm_q12());
}

// --- Soft fail-safe (spec §4.7) -------------------------------------

void test_failsafe_does_not_trigger_below_5s(void) {
    drive(0, 4000, 0);   // 4s of zero
    TEST_ASSERT_FALSE(dimmer_failsafe_active());
}

void test_failsafe_triggers_after_5s_of_zero(void) {
    drive(0, 6000, 0);
    TEST_ASSERT_TRUE(dimmer_failsafe_active());
    TEST_ASSERT_EQUAL(2048, dimmer_get_norm_q12());  // 50%
}

void test_failsafe_recovers_when_input_returns(void) {
    drive(0, 6000, 0);
    TEST_ASSERT_TRUE(dimmer_failsafe_active());

    drive(6000, 1000, 2000);
    TEST_ASSERT_FALSE(dimmer_failsafe_active());
}

void test_brief_zero_does_not_trigger_failsafe(void) {
    uint32_t t = 0;
    t = drive(t, 1000, 2000);   // 1s stable
    t = drive(t, 2000, 0);      // 2s of zero (below 5s threshold)
    t = drive(t, 1000, 2000);   // recovers
    TEST_ASSERT_FALSE(dimmer_failsafe_active());
}

void test_failsafe_threshold_allows_small_noise(void) {
    // raw=4 is below FAILSAFE_RAW_THRESHOLD (5) — should still trigger eventually
    drive(0, 6000, 4);
    TEST_ASSERT_TRUE(dimmer_failsafe_active());
}

void test_failsafe_does_not_trigger_just_above_threshold(void) {
    drive(0, 6000, 6);  // raw=6 > threshold
    TEST_ASSERT_FALSE(dimmer_failsafe_active());
}

// --- Test runner -----------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_zero);
    RUN_TEST(test_converges_to_max_at_full_bright);
    RUN_TEST(test_converges_to_mid_at_mid_input);
    RUN_TEST(test_rate_limited_to_50hz);
    RUN_TEST(test_deadband_at_low_end_clamps_to_zero);
    RUN_TEST(test_deadband_at_high_end_clamps_to_max);
    RUN_TEST(test_failsafe_does_not_trigger_below_5s);
    RUN_TEST(test_failsafe_triggers_after_5s_of_zero);
    RUN_TEST(test_failsafe_recovers_when_input_returns);
    RUN_TEST(test_brief_zero_does_not_trigger_failsafe);
    RUN_TEST(test_failsafe_threshold_allows_small_noise);
    RUN_TEST(test_failsafe_does_not_trigger_just_above_threshold);
    return UNITY_END();
}
