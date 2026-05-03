// test/test_aux_led/test_aux_led.c
#include <unity.h>
#include "aux_led.h"

void setUp(void)    { aux_led_init(); }
void tearDown(void) {}

void test_zero_dim_is_off(void) {
    TEST_ASSERT_EQUAL(0, aux_led_compute_duty(0));
}

void test_full_bright_passes_through(void) {
    TEST_ASSERT_EQUAL(4095, aux_led_compute_duty(4095));
}

void test_midrange_passes_through(void) {
    TEST_ASSERT_EQUAL(2048, aux_led_compute_duty(2048));
}

void test_no_floor_applied(void) {
    // Aux LED has NO MC_FLOOR — even tiny dimmer values pass through unchanged.
    TEST_ASSERT_EQUAL(1,   aux_led_compute_duty(1));
    TEST_ASSERT_EQUAL(100, aux_led_compute_duty(100));
    TEST_ASSERT_EQUAL(613, aux_led_compute_duty(613));   // just under MC_FLOOR
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_dim_is_off);
    RUN_TEST(test_full_bright_passes_through);
    RUN_TEST(test_midrange_passes_through);
    RUN_TEST(test_no_floor_applied);
    return UNITY_END();
}
