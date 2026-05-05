// test/test_button/test_button.c
#include <unity.h>
#include "button.h"

#define DEBOUNCE_MS    50
#define LONG_PRESS_MS  3000

void setUp(void)    { button_init(); }
void tearDown(void) {}

// --- No press --------------------------------------------------------

void test_no_press_no_event(void) {
    for (uint32_t t = 0; t < 1000; t += 10) button_tick(t, false);
    TEST_ASSERT_FALSE(button_consume_press());
}

void test_held_release_alone_no_event(void) {
    for (uint32_t t = 0; t < 200; t += 10) button_tick(t, false);
    TEST_ASSERT_FALSE(button_consume_press());
}

// --- Qualified press -------------------------------------------------

void test_qualified_press_emits_event(void) {
    button_tick(0,  true);
    button_tick(49, true);
    TEST_ASSERT_FALSE(button_consume_press());
    button_tick(50, true);
    TEST_ASSERT_TRUE(button_consume_press());
}

void test_consume_clears_event(void) {
    button_tick(0,  true);
    button_tick(50, true);
    TEST_ASSERT_TRUE (button_consume_press());
    TEST_ASSERT_FALSE(button_consume_press());   // cleared
}

void test_held_button_only_emits_once(void) {
    for (uint32_t t = 0; t <= 1000; t += 10) button_tick(t, true);
    TEST_ASSERT_TRUE (button_consume_press());
    TEST_ASSERT_FALSE(button_consume_press());
}

// --- Glitches & chatter ----------------------------------------------

void test_short_glitch_does_not_emit(void) {
    button_tick(0,  true);
    button_tick(20, true);    // glitch ends at 20ms — under threshold
    button_tick(20, false);
    button_tick(100, false);
    TEST_ASSERT_FALSE(button_consume_press());
}

void test_chatter_resets_qualification_timer(void) {
    button_tick(0,  true);
    button_tick(40, true);    // 40ms qualifying
    button_tick(41, false);   // glitch back, cancels transition
    button_tick(42, true);    // restarts qualification at 42
    button_tick(91, true);    // 49ms in — not yet
    TEST_ASSERT_FALSE(button_consume_press());
    button_tick(92, true);    // 50ms — qualifies
    TEST_ASSERT_TRUE(button_consume_press());
}

// --- Multiple presses -------------------------------------------------

void test_two_separate_presses_emit_twice(void) {
    button_tick(0,   true);
    button_tick(50,  true);   // press 1 qualifies
    TEST_ASSERT_TRUE(button_consume_press());

    button_tick(60,  false);
    button_tick(110, false);  // release qualifies

    button_tick(200, true);
    button_tick(250, true);   // press 2 qualifies
    TEST_ASSERT_TRUE(button_consume_press());
}

// --- Long press ------------------------------------------------------

void test_short_hold_no_long_press(void) {
    button_tick(0,    true);
    button_tick(50,   true);   // qualified
    button_tick(2999, true);   // just under threshold
    TEST_ASSERT_FALSE(button_consume_long_press());
}

void test_long_press_fires_at_threshold(void) {
    button_tick(0,    true);
    button_tick(50,   true);   // qualified at t=50; pressed_since=50
    button_tick(3050, true);   // 3000ms elapsed → fires
    TEST_ASSERT_TRUE(button_consume_long_press());
}

void test_long_press_fires_only_once_while_held(void) {
    button_tick(0,    true);
    button_tick(50,   true);
    button_tick(3050, true);   // fires
    (void)button_consume_long_press();
    button_tick(4000, true);   // still held — must not re-fire
    TEST_ASSERT_FALSE(button_consume_long_press());
}

void test_long_press_consume_clears(void) {
    button_tick(0,    true);
    button_tick(50,   true);
    button_tick(3050, true);
    TEST_ASSERT_TRUE (button_consume_long_press());
    TEST_ASSERT_FALSE(button_consume_long_press());
}

void test_short_press_still_fires_on_same_hold(void) {
    button_tick(0,    true);
    button_tick(50,   true);   // short press qualifies here
    button_tick(3050, true);   // long press fires here
    TEST_ASSERT_TRUE(button_consume_press());
    TEST_ASSERT_TRUE(button_consume_long_press());
}

void test_release_before_threshold_clears_long_press(void) {
    button_tick(0,    true);
    button_tick(50,   true);   // qualified
    button_tick(100,  false);  // released early
    button_tick(150,  false);  // release qualifies
    button_tick(3500, false);  // held long past threshold but not pressed
    TEST_ASSERT_FALSE(button_consume_long_press());
}

void test_long_press_rearms_on_second_press(void) {
    // First press — hold past threshold.
    button_tick(0,    true);
    button_tick(50,   true);
    button_tick(3050, true);
    (void)button_consume_long_press();

    // Release and re-press.
    button_tick(3060, false);
    button_tick(3110, false);  // release qualifies
    button_tick(3200, true);
    button_tick(3250, true);   // new press qualifies; pressed_since=3250
    button_tick(6250, true);   // 3000ms later → long press again
    TEST_ASSERT_TRUE(button_consume_long_press());
}

void test_release_alone_does_not_emit(void) {
    // Get to qualified pressed state and consume.
    button_tick(0,  true);
    button_tick(50, true);
    (void)button_consume_press();

    // Now qualified release.
    button_tick(60,  false);
    button_tick(110, false);
    TEST_ASSERT_FALSE(button_consume_press());   // release ≠ press
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_press_no_event);
    RUN_TEST(test_held_release_alone_no_event);
    RUN_TEST(test_qualified_press_emits_event);
    RUN_TEST(test_consume_clears_event);
    RUN_TEST(test_held_button_only_emits_once);
    RUN_TEST(test_short_glitch_does_not_emit);
    RUN_TEST(test_chatter_resets_qualification_timer);
    RUN_TEST(test_two_separate_presses_emit_twice);
    RUN_TEST(test_release_alone_does_not_emit);
    RUN_TEST(test_short_hold_no_long_press);
    RUN_TEST(test_long_press_fires_at_threshold);
    RUN_TEST(test_long_press_fires_only_once_while_held);
    RUN_TEST(test_long_press_consume_clears);
    RUN_TEST(test_short_press_still_fires_on_same_hold);
    RUN_TEST(test_release_before_threshold_clears_long_press);
    RUN_TEST(test_long_press_rearms_on_second_press);
    return UNITY_END();
}
