// test/test_led_controller/test_led_controller.c
//
// Unity tests for the LED controller.
// Run on the host: pio test -e host -f test_led_controller

#include <unity.h>
#include "led_controller.h"

// Gamma=2.2 mapped floors: 15% → brightness_u8=38 → lut=4 → q12=64
//                          35% → brightness_u8=89 → lut=25 → q12=401
#define FLOOR_ACK_Q12     64u
#define FLOOR_PENDING_Q12 401u

static led_drive_t out;

void setUp(void) {
    led_controller_init();
    out = (led_drive_t){0, 0, 0};
}
void tearDown(void) {}

// --- No alarms ----------------------------------------------------------

void test_no_alarm_all_off(void) {
    led_controller_tick(0, SEV_NONE, false, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_no_alarm_pending_flag_ignored(void) {
    // SEV_NONE wins regardless of any_pending
    led_controller_tick(0, SEV_NONE, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
}

// --- Severity → color (acknowledged, full bright) ----------------------

void test_high_acked_steady_red(void) {
    led_controller_tick(0, SEV_HIGH, false, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_med_acked_steady_amber(void) {
    led_controller_tick(0, SEV_MED, false, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

void test_low_acked_steady_green(void) {
    led_controller_tick(0, SEV_LOW, false, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);
    TEST_ASSERT_EQUAL(0, out.blue);
}

// --- Flash modulation (pending) ---------------------------------------

void test_pending_at_t0_is_on(void) {
    led_controller_tick(0, SEV_HIGH, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
}

void test_pending_at_t250_is_off(void) {
    led_controller_tick(250, SEV_HIGH, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
}

void test_flash_cycle_full_period(void) {
    // Walk through one full 500 ms cycle and into the next.
    const uint32_t pts[] = { 0, 249, 250, 499, 500, 749, 750 };
    const bool     on[]  = { 1,    1,   0,   0,   1,   1,   0 };
    for (size_t i = 0; i < sizeof(pts)/sizeof(pts[0]); i++) {
        led_controller_tick(pts[i], SEV_HIGH, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
        if (on[i]) {
            TEST_ASSERT_EQUAL_MESSAGE(4095, out.red, "expected ON");
        } else {
            TEST_ASSERT_EQUAL_MESSAGE(0,    out.red, "expected OFF");
        }
    }
}

void test_pending_amber_both_cathodes_modulated_in_phase(void) {
    led_controller_tick(0, SEV_MED, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
    TEST_ASSERT_EQUAL(4095, out.green);

    led_controller_tick(250, SEV_MED, true, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
    TEST_ASSERT_EQUAL(0, out.green);
}

// --- Brightness floor (spec §4.7) ------------------------------------

void test_dimmer_zero_clamped_to_floor(void) {
    led_controller_tick(0, SEV_HIGH, false, 0, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_ACK_Q12, out.red);
}

void test_dimmer_just_below_floor_clamped(void) {
    led_controller_tick(0, SEV_HIGH, false, FLOOR_ACK_Q12 - 1, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_ACK_Q12, out.red);
}

void test_dimmer_at_floor_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, FLOOR_ACK_Q12, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_ACK_Q12, out.red);
}

void test_dimmer_above_floor_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, 2048, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(2048, out.red);
}

void test_dimmer_full_passes_through(void) {
    led_controller_tick(0, SEV_HIGH, false, 4095, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(4095, out.red);
}

// --- Combined: dimmer + flash ---------------------------------------

void test_pending_flash_uses_dimmed_duty_in_on_phase(void) {
    led_controller_tick(0, SEV_LOW, true, 1500, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(1500, out.green);  // ON, dimmer pass-through

    led_controller_tick(250, SEV_LOW, true, 1500, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.green);     // OFF
}

void test_pending_flash_with_dim_below_floor_uses_pending_floor(void) {
    // ON phase at dimmer=0: clamped to higher pending floor (35% gamma-mapped)
    led_controller_tick(0, SEV_HIGH, true, 0, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_PENDING_Q12, out.red);

    // OFF phase stays dark regardless of floor
    led_controller_tick(250, SEV_HIGH, true, 0, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(0, out.red);
}

void test_acked_alarm_uses_lower_floor(void) {
    // Acked alarm (any_pending=false) uses 15% gamma floor, not 35%
    led_controller_tick(0, SEV_HIGH, false, 0, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_ACK_Q12, out.red);
}

void test_pending_floor_above_acked_floor(void) {
    TEST_ASSERT_GREATER_THAN(FLOOR_ACK_Q12, FLOOR_PENDING_Q12);
}

void test_pending_dimmer_between_floors_uses_pending_floor(void) {
    // dimmer=200 (above acked 64 but below pending 401) → should clamp to pending floor
    led_controller_tick(0, SEV_HIGH, true, 200, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(FLOOR_PENDING_Q12, out.red);
}

void test_pending_dimmer_above_pending_floor_passes_through(void) {
    // dimmer above both floors → passes through unchanged
    led_controller_tick(0, SEV_HIGH, true, 2000, FLOOR_ACK_Q12, FLOOR_PENDING_Q12, &out);
    TEST_ASSERT_EQUAL(2000, out.red);
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
    RUN_TEST(test_pending_flash_with_dim_below_floor_uses_pending_floor);
    RUN_TEST(test_acked_alarm_uses_lower_floor);
    RUN_TEST(test_pending_floor_above_acked_floor);
    RUN_TEST(test_pending_dimmer_between_floors_uses_pending_floor);
    RUN_TEST(test_pending_dimmer_above_pending_floor_passes_through);
    return UNITY_END();
}
