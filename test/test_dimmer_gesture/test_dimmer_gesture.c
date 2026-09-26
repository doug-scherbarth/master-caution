// test/test_dimmer_gesture/test_dimmer_gesture.c
// Unity tests for the dimmer_gesture module.
// Run: pio test -e host -f test_dimmer_gesture

#include <unity.h>
#include "dimmer_gesture.h"

// Test parameters — kept as percentages to match LIGHTS.CFG
#define LOW_PCT   20u
#define HIGH_PCT  80u
#define T_OUT_MS  2000u

// Threshold values in 0-255 ratio space (mirrors init conversion)
#define LOW  ((uint8_t)(LOW_PCT  * 255u / 100u))   // 51
#define HIGH ((uint8_t)(HIGH_PCT * 255u / 100u))   // 204
#define T_OUT T_OUT_MS

// Mid-range value safely inside the neutral zone
#define MID 128u

void setUp(void)    { dimmer_gesture_init(LOW_PCT, HIGH_PCT, T_OUT_MS); }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// No gesture in neutral zone
// ---------------------------------------------------------------------------

void test_stable_mid_no_gesture(void) {
    for (int i = 0; i < 100; i++)
        TEST_ASSERT_FALSE(dimmer_gesture_tick(i * 10, MID));
}

// ---------------------------------------------------------------------------
// Dip gesture (cross below LOW, return above LOW, within timeout)
// ---------------------------------------------------------------------------

void test_dip_fires_on_return(void) {
    dimmer_gesture_tick(0, MID);           // idle in neutral
    dimmer_gesture_tick(10, LOW - 1);      // cross below LOW — arm
    bool fired = dimmer_gesture_tick(20, LOW);  // return to LOW — gesture!
    TEST_ASSERT_TRUE(fired);
}

void test_dip_fires_once_not_repeatedly(void) {
    dimmer_gesture_tick(0,  MID);
    dimmer_gesture_tick(10, LOW - 1);
    bool first  = dimmer_gesture_tick(20, LOW);   // gesture fires
    bool second = dimmer_gesture_tick(30, MID);   // back in neutral, no repeat
    TEST_ASSERT_TRUE(first);
    TEST_ASSERT_FALSE(second);
}

void test_dip_timeout_no_gesture(void) {
    dimmer_gesture_tick(0, MID);
    dimmer_gesture_tick(10, LOW - 1);              // arm
    // stay below LOW past the timeout
    bool fired = dimmer_gesture_tick(10 + T_OUT + 1, LOW - 1);
    TEST_ASSERT_FALSE(fired);
    // now return — still no gesture (already timed out)
    fired = dimmer_gesture_tick(10 + T_OUT + 2, LOW);
    TEST_ASSERT_FALSE(fired);
}

void test_dip_return_exactly_at_low_threshold(void) {
    dimmer_gesture_tick(0,  MID);
    dimmer_gesture_tick(10, LOW - 1);
    // Return to exactly LOW (>= LOW threshold)
    TEST_ASSERT_TRUE(dimmer_gesture_tick(20, LOW));
}

void test_dip_from_zero_ratio(void) {
    // ratio starts at 0 (already below LOW) from boot — should arm immediately
    dimmer_gesture_tick(0, 0);
    TEST_ASSERT_TRUE(dimmer_gesture_tick(100, MID));
}

// ---------------------------------------------------------------------------
// Bump gesture (cross above HIGH, return below HIGH, within timeout)
// ---------------------------------------------------------------------------

void test_bump_fires_on_return(void) {
    dimmer_gesture_tick(0, MID);
    dimmer_gesture_tick(10, HIGH + 1);
    TEST_ASSERT_TRUE(dimmer_gesture_tick(20, HIGH));
}

void test_bump_fires_once_not_repeatedly(void) {
    dimmer_gesture_tick(0, MID);
    dimmer_gesture_tick(10, HIGH + 1);
    bool first  = dimmer_gesture_tick(20, HIGH);
    bool second = dimmer_gesture_tick(30, MID);
    TEST_ASSERT_TRUE(first);
    TEST_ASSERT_FALSE(second);
}

void test_bump_timeout_no_gesture(void) {
    dimmer_gesture_tick(0, MID);
    dimmer_gesture_tick(10, HIGH + 1);
    dimmer_gesture_tick(10 + T_OUT + 1, HIGH + 1);  // still above HIGH, timed out
    TEST_ASSERT_FALSE(dimmer_gesture_tick(10 + T_OUT + 2, HIGH));
}

void test_bump_return_exactly_at_high_threshold(void) {
    dimmer_gesture_tick(0, MID);
    dimmer_gesture_tick(10, HIGH + 1);
    TEST_ASSERT_TRUE(dimmer_gesture_tick(20, HIGH));
}

// ---------------------------------------------------------------------------
// Timeout boundary: fires if returned exactly at timeout tick
// (timeout is strict greater-than so T_OUT exactly should still arm)
// ---------------------------------------------------------------------------

void test_dip_return_at_exactly_timeout_still_fires(void) {
    dimmer_gesture_tick(1000, MID);
    dimmer_gesture_tick(1010, LOW - 1);               // arm at t=1010
    // return at exactly arm_time + T_OUT (not yet expired)
    TEST_ASSERT_TRUE(dimmer_gesture_tick(1010 + T_OUT, LOW));
}

void test_dip_expired_one_ms_past_timeout(void) {
    dimmer_gesture_tick(1000, MID);
    dimmer_gesture_tick(1010, LOW - 1);
    // tick that expires the arm (still below LOW — clears to IDLE)
    dimmer_gesture_tick(1010 + T_OUT + 1, LOW - 1);
    // return: already idle, no gesture
    TEST_ASSERT_FALSE(dimmer_gesture_tick(1010 + T_OUT + 2, LOW));
}

// ---------------------------------------------------------------------------
// Re-arm after timeout: next gesture should still work
// ---------------------------------------------------------------------------

void test_dip_rearms_after_timeout(void) {
    // First attempt — let it timeout
    dimmer_gesture_tick(0,  MID);
    dimmer_gesture_tick(10, LOW - 1);
    dimmer_gesture_tick(10 + T_OUT + 5, LOW - 1);  // expire
    dimmer_gesture_tick(10 + T_OUT + 6, MID);       // back to neutral

    // Second attempt — quick dip
    dimmer_gesture_tick(5000, MID);
    dimmer_gesture_tick(5010, LOW - 1);
    TEST_ASSERT_TRUE(dimmer_gesture_tick(5020, LOW));
}

// ---------------------------------------------------------------------------
// Two gestures in sequence both fire
// ---------------------------------------------------------------------------

void test_two_gestures_in_sequence(void) {
    // First gesture: dip
    dimmer_gesture_tick(0,   MID);
    dimmer_gesture_tick(10,  LOW - 1);
    bool g1 = dimmer_gesture_tick(20, LOW);

    // Second gesture: bump (some time later)
    dimmer_gesture_tick(1000, MID);
    dimmer_gesture_tick(1010, HIGH + 1);
    bool g2 = dimmer_gesture_tick(1020, HIGH);

    TEST_ASSERT_TRUE(g1);
    TEST_ASSERT_TRUE(g2);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_stable_mid_no_gesture);
    RUN_TEST(test_dip_fires_on_return);
    RUN_TEST(test_dip_fires_once_not_repeatedly);
    RUN_TEST(test_dip_timeout_no_gesture);
    RUN_TEST(test_dip_return_exactly_at_low_threshold);
    RUN_TEST(test_dip_from_zero_ratio);
    RUN_TEST(test_bump_fires_on_return);
    RUN_TEST(test_bump_fires_once_not_repeatedly);
    RUN_TEST(test_bump_timeout_no_gesture);
    RUN_TEST(test_bump_return_exactly_at_high_threshold);
    RUN_TEST(test_dip_return_at_exactly_timeout_still_fires);
    RUN_TEST(test_dip_expired_one_ms_past_timeout);
    RUN_TEST(test_dip_rearms_after_timeout);
    RUN_TEST(test_two_gestures_in_sequence);

    return UNITY_END();
}
