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
        dimmer_tick(t, raw, 4095);
        t += 20;
    }
    return t;
}

void setUp(void) { dimmer_init(); }
void tearDown(void) {}

// --- Initial state ----------------------------------------------------

void test_initial_state_zero(void) {
    TEST_ASSERT_EQUAL(0, dimmer_get_norm_q12());
}

// --- IIR convergence --------------------------------------------------

void test_converges_to_max_at_full_bright(void) {
    drive(0, 2000, 4095);  // ratio=1.0 → above 97% deadband → full bright
    TEST_ASSERT_EQUAL(4095, dimmer_get_norm_q12());
}

void test_converges_to_mid_at_mid_input(void) {
    drive(0, 2000, 2048);  // ratio≈0.5 → midpoint between deadbands
    TEST_ASSERT_INT_WITHIN(50, 2048, dimmer_get_norm_q12());
}

// --- Rate limiting ----------------------------------------------------

void test_rate_limited_to_50hz(void) {
    dimmer_tick(0, 4095, 4095);
    const uint16_t after_first = dimmer_get_norm_q12();

    // Within rate-limit window — no further sampling
    dimmer_tick(5,  4095, 4095);
    dimmer_tick(10, 4095, 4095);
    dimmer_tick(19, 4095, 4095);
    TEST_ASSERT_EQUAL(after_first, dimmer_get_norm_q12());

    // Crossing 20ms threshold — new sample taken
    dimmer_tick(20, 4095, 4095);
    TEST_ASSERT_GREATER_THAN(after_first, dimmer_get_norm_q12());
}

// --- Deadbands -------------------------------------------------------

void test_deadband_at_low_end_clamps_to_zero(void) {
    // ratio=100 < DIM_DEADBAND_LOW(123) → clamps to 0
    drive(0, 2000, 100);
    TEST_ASSERT_EQUAL(0, dimmer_get_norm_q12());
}

void test_deadband_at_high_end_clamps_to_max(void) {
    // ratio=4000 > DIM_DEADBAND_HIGH(3972) → clamps to 4095
    drive(0, 2000, 4000);
    TEST_ASSERT_EQUAL(4095, dimmer_get_norm_q12());
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
    return UNITY_END();
}
