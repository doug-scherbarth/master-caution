// test/test_regress_gesture/test_regress_gesture.c
// Regression tests for review item 1: gesture must be edge-triggered with
// hysteresis. Normal knob use (parking low/high, slow moves, ADC noise at a
// threshold) must never fire a gesture.
//
// Contract (TASKS.md, Task 2):
//   - Arms only on a TRANSITION from outside a zone to inside it.
//     The first sample after init establishes the zone and never arms.
//   - A dip completes only when the ratio returns to >= low + HYST.
//     A bump completes only when the ratio returns to <= high - HYST.
//   - "Outside" for re-arming uses the same hysteresis thresholds.
//   - HYST = 5 percentage points (13 counts on the 0..255 scale).

#include <unity.h>
#include "dimmer_gesture.h"

#define LOW    51u    // 20 % of 255
#define HIGH  204u    // 80 % of 255
#define HYST   13u    //  5 % of 255
#define MID   128u
#define STEP   20u    // 50 Hz frame

static int g_fired;

// Hold a constant ratio for dur_ms, return end time.
static uint32_t hold(uint32_t t0, uint32_t dur_ms, uint8_t r) {
    for (uint32_t t = t0; t < t0 + dur_ms; t += STEP) g_fired += dimmer_gesture_tick(t, r);
    return t0 + dur_ms;
}

// Linear ramp from a to b over dur_ms, return end time.
static uint32_t ramp(uint32_t t0, uint32_t dur_ms, uint8_t a, uint8_t b) {
    uint32_t n = dur_ms / STEP;
    for (uint32_t k = 0; k < n; k++) {
        int32_t r = (int32_t)a + ((int32_t)b - (int32_t)a) * (int32_t)k / (int32_t)n;
        g_fired += dimmer_gesture_tick(t0 + k * STEP, (uint8_t)r);
    }
    return t0 + dur_ms;
}

void setUp(void)    { dimmer_gesture_init(20, 80, 2000); g_fired = 0; }
void tearDown(void) {}

void test_parked_low_then_slow_raise_no_gesture(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 60000, 26);          // night: knob parked at ~10 %
    ramp(t, 10000, 26, MID);         // pilot slowly brings lights up
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_parked_low_at_boot_then_quick_raise_no_gesture(void) {
    uint32_t t = hold(0, 5000, 26);  // first sample is already low: must not arm
    hold(t, 1000, MID);
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_parked_high_then_slow_lower_no_gesture(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 60000, 250);         // day: full bright
    ramp(t, 10000, 250, 150);
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_noise_at_low_threshold_no_gesture(void) {
    uint32_t t = hold(0, 1000, MID);
    for (uint32_t k = 0; k < 500; k++, t += STEP)       // 10 s of +/-2 LSB noise
        g_fired += dimmer_gesture_tick(t, (uint8_t)(LOW - 2u + (k % 5u)));
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_noise_at_high_threshold_no_gesture(void) {
    uint32_t t = hold(0, 1000, MID);
    for (uint32_t k = 0; k < 500; k++, t += STEP)
        g_fired += dimmer_gesture_tick(t, (uint8_t)(HIGH - 2u + (k % 5u)));
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_quick_dip_from_mid_fires_once(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 400, 10);
    hold(t, 1000, MID);
    TEST_ASSERT_EQUAL_INT(1, g_fired);
}

void test_quick_bump_from_mid_fires_once(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 400, 250);
    hold(t, 1000, MID);
    TEST_ASSERT_EQUAL_INT(1, g_fired);
}

void test_dip_return_inside_hysteresis_does_not_fire(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 200, 10);
    hold(t, 3000, (uint8_t)(LOW + HYST - 1u));  // above LOW but not past hysteresis
    TEST_ASSERT_EQUAL_INT(0, g_fired);
}

void test_dip_return_past_hysteresis_fires(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 200, 10);
    hold(t, 500, (uint8_t)(LOW + HYST));
    TEST_ASSERT_EQUAL_INT(1, g_fired);
}

void test_cancelled_dip_requires_exit_and_reentry(void) {
    uint32_t t = hold(0, 1000, MID);
    t = hold(t, 3000, 10);           // held past timeout: cancelled
    t = hold(t, 1000, MID);          // leaving the zone after a cancel must NOT fire
    TEST_ASSERT_EQUAL_INT(0, g_fired);
    t = hold(t, 300, 10);            // a fresh, quick dip still works
    hold(t, 500, MID);
    TEST_ASSERT_EQUAL_INT(1, g_fired);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parked_low_then_slow_raise_no_gesture);
    RUN_TEST(test_parked_low_at_boot_then_quick_raise_no_gesture);
    RUN_TEST(test_parked_high_then_slow_lower_no_gesture);
    RUN_TEST(test_noise_at_low_threshold_no_gesture);
    RUN_TEST(test_noise_at_high_threshold_no_gesture);
    RUN_TEST(test_quick_dip_from_mid_fires_once);
    RUN_TEST(test_quick_bump_from_mid_fires_once);
    RUN_TEST(test_dip_return_inside_hysteresis_does_not_fire);
    RUN_TEST(test_dip_return_past_hysteresis_fires);
    RUN_TEST(test_cancelled_dip_requires_exit_and_reentry);
    return UNITY_END();
}
