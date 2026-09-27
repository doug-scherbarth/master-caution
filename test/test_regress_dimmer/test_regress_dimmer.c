// test/test_regress_dimmer/test_regress_dimmer.c
// Regression for review item 4: knob fully down is a legitimate setting.
// The "soft fail-safe" (ratio ~0 for 5 s -> 50 %) must be removed; this
// circuit cannot tell an open wire from a knob at zero.

#include <unity.h>
#include "dimmer.h"

#define BUS_12V 3039u   // ~12 V through 39k/10k divider, 12-bit ADC

void setUp(void)    { dimmer_init(); }
void tearDown(void) {}

void test_knob_at_zero_stays_zero_indefinitely(void) {
    for (uint32_t t = 0; t <= 60000u; t += 20u) dimmer_tick(t, 0u, BUS_12V);
    TEST_ASSERT_EQUAL_UINT16(0u, dimmer_get_norm_q12());
}

void test_knob_at_zero_after_being_up_stays_zero(void) {
    uint32_t t = 0;
    for (; t < 2000u;  t += 20u) dimmer_tick(t, 1500u, BUS_12V);
    for (; t < 20000u; t += 20u) dimmer_tick(t, 0u,    BUS_12V);
    TEST_ASSERT_EQUAL_UINT16(0u, dimmer_get_norm_q12());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_knob_at_zero_stays_zero_indefinitely);
    RUN_TEST(test_knob_at_zero_after_being_up_stays_zero);
    return UNITY_END();
}
