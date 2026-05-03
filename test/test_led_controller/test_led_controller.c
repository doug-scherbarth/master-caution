// test/test_led_controller/test_led_controller.c
//
// Unity tests for the LED controller.
// Run on the host: pio test -e host -f test_led_controller

#include <unity.h>
#include "led_controller.h"

#define MC_FLOOR_Q12 614   // mirror of internal constant for clarity in tests

static led_drive_t out;

void setUp(void) {
    led_controller_init();
    out = (led_drive_t){0, 0, 0};
}
void tearDown(void) {}

// --- No alarms ----------------------------------------------------------

void test_no_alarm_all_off(void) {
    led_controller_tick(0, SEV_NONE, false, 4095, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_no_alarm_pending_flag_ignored(void) {
    // SEV_NONE wins regardless of any_pending
    led_controller_tick(0, SEV_NONE, true, 4095, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
}

// --- Severity → color (acknowledged, full bright) ----------------------

void test_high_acked_steady_red(void) {
    led_controller_tick(0, SEV_HIGH, false, 4095, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_med_acked_steady_amber(void) {
    led_controller_tick(0, SEV_MED, false, 4095, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_low_acked_steady_green(void) {
    led_controller_tick(0, SEV_LOW, false, 4095, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

// --- Flash modulation (pending) ---------------------------------------

void test_pending_at_t0_is_on(void) {
    led_controller_tick(0, SEV_HIGH, true, 4095, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
}

void test_pending_at_t250_is_off(void) {
    led_controller_tick(250, SEV_HIGH, true, 4095, &out);
    TEST_ASSERT_EQUAL(0, out.red);
}

void test_flash_cycle_full_period(void) {
    // Walk through one full 500 ms cycle and into the next.
    const uint32_t pts[] = { 0, 249, 250, 499, 500, 749, 750 };
    const bool     on[]  = { 1,    1,   0,   0,   1,   1,   0 };
    for (size_t i = 0; i < sizeof(pts)/sizeof(pts[0]); i++) {
        led_controller_tick(pts[i], SEV_HIGH, true, 4095, &out);
        if (on[i]) {
            TEST_ASSERT_EQUAL_MESSAGE(4095, out.red, "expected ON");
        } else {
            TEST_ASSERT_EQUAL_MESSAGE(0,    out.red, "expected OFF");
        }
    }
}

void test_pending_amber_both_cathodes_modulated_in_phase(void) {
    led_controller_tick(0, SEV_MED, true, 4095, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);

    led_controller_tick(250, SEV_MED, true, 4095, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
}

// --- Brightness floor (spec §4.7) ------------------------------------

void test_dimmer_zero_clamped_to_floor(void) {
    led_controller_tick(0, SEV_HIGH, false, 0, &out);
    TEST_ASSERT_EQUAL(MC_FLOOR_Q12, out.red);
}

void test_dimmer_just_below_floor_clamped(void) {
    led_controller_tick(0, SEV_HIGH, false, MC_FLOOR_Q12 - 1, &out);
    TEST_ASSERT_EQUAL(MC_FLOOR_Q12, out.red);
}

void test_dimmer_at_floor_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, MC_FLOOR_Q12, &out);
    TEST_ASSERT_EQUAL(MC_FLOOR_Q12, out.red);
}

void test_dimmer_above_floor_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, 2048, &out);
    TEST_ASSERT_EQUAL(2048, out.red);
}

void test_dimmer_full_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, 4095, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
}

// --- Combined: dimmer + flash ---------------------------------------

void test_pending_flash_uses_dimmed_duty_in_on_phase(void) {
    led_controller_tick(0, SEV_LOW, true, 1500, &out);
    TEST_ASSERT_EQUAL(1500, out.green);  // ON, dimmer pass-through

    led_controller_tick(250, SEV_LOW, true, 1500, &out);
    TEST_ASSERT_EQUAL(0, out.green);     // OFF
}

void test_pending_flash_with_dim_below_floor_uses_floor(void) {
    led_controller_tick(0, SEV_HIGH, true, 0, &out);
    TEST_ASSERT_EQUAL(MC_FLOOR_Q12, out.red);  // ON, clamped to floor

    led_controller_tick(250, SEV_HIGH, true, 0, &out);
    TEST_ASSERT_EQUAL(0, out.red);             // OFF
}

// --- Test runner ----------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_alarm_all_off);
    RUN_TEST(test_no_alarm_pending_flag_ignored);
    RUN_TEST(test_high_acked_steady_red);
    RUN_TEST(test_med_acked_steady_amber);
    RUN_TEST(test_low_acked_steady_green);
    RUN_TEST(test_pending_at_t0_is_on);
    RUN_TEST(test_pending_at_t250_is_off);
    RUN_TEST(test_flash_cycle_full_period);
    RUN_TEST(test_pending_amber_both_cathodes_modulated_in_phase);
    RUN_TEST(test_dimmer_zero_clamped_to_floor);
    RUN_TEST(test_dimmer_just_below_floor_clamped);
    RUN_TEST(test_dimmer_at_floor_passes_through);
    RUN_TEST(test_dimmer_above_floor_passes_through);
    RUN_TEST(test_dimmer_full_passes_through);
    RUN_TEST(test_pending_flash_uses_dimmed_duty_in_on_phase);
    RUN_TEST(test_pending_flash_with_dim_below_floor_uses_floor);
    return UNITY_END();
}
