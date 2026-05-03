// test/test_ring_log/test_ring_log.c
#include <unity.h>
#include <string.h>
#include "ring_log.h"
#include "alarm_table.h"

extern char   hal_log_buf[];
extern size_t hal_log_buf_len;
void hal_mock_reset(void);

// Find alarm by name so tests are robust to table reordering.
static uint8_t find_alarm(const char *name) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (strcmp(ALARM_TABLE[i].name, name) == 0) return i;
    }
    TEST_FAIL_MESSAGE("alarm not found");
    return 0xFF;
}

void setUp(void) {
    ring_log_init();
    hal_mock_reset();
}
void tearDown(void) {}

// --- Basics --------------------------------------------------------

void test_initial_state_empty(void) {
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());
    TEST_ASSERT_EQUAL(0, ring_log_dropped_count());
    ring_log_tick();
    TEST_ASSERT_EQUAL(0u, hal_log_buf_len);
}

void test_single_event_drains(void) {
    ring_log_alarm(LOG_ALARM_ASSERT, find_alarm("CO_DETECT"), 1234);
    TEST_ASSERT_EQUAL(1, ring_log_pending_count());

    ring_log_tick();
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "CO_DETECT"));
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "ASSERT"));
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "1234"));
}

// --- One-record-per-tick rate limit -------------------------------

void test_drains_one_per_tick(void) {
    ring_log_alarm(LOG_ALARM_ASSERT, find_alarm("CO_DETECT"),     100);
    ring_log_alarm(LOG_ALARM_ACK,    find_alarm("CO_DETECT"),     200);
    ring_log_alarm(LOG_ALARM_CLEAR,  find_alarm("CO_DETECT"),     300);
    TEST_ASSERT_EQUAL(3, ring_log_pending_count());

    ring_log_tick();
    TEST_ASSERT_EQUAL(2, ring_log_pending_count());

    ring_log_tick();
    TEST_ASSERT_EQUAL(1, ring_log_pending_count());

    ring_log_tick();
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());

    ring_log_tick();   // nothing more
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());
}

void test_event_ordering_preserved(void) {
    ring_log_alarm(LOG_ALARM_ASSERT, find_alarm("CO_DETECT"),    100);
    ring_log_alarm(LOG_ALARM_ASSERT, find_alarm("OIL_PRESS_LOW"),200);
    ring_log_alarm(LOG_ALARM_ASSERT, find_alarm("CHT_OVERTEMP"), 300);

    for (int i = 0; i < 3; i++) ring_log_tick();

    // CO appears before OIL_PRESS in buffer; OIL_PRESS before CHT.
    char *p_co  = strstr(hal_log_buf, "CO_DETECT");
    char *p_oil = strstr(hal_log_buf, "OIL_PRESS_LOW");
    char *p_cht = strstr(hal_log_buf, "CHT_OVERTEMP");
    TEST_ASSERT_NOT_NULL(p_co);
    TEST_ASSERT_NOT_NULL(p_oil);
    TEST_ASSERT_NOT_NULL(p_cht);
    TEST_ASSERT_TRUE(p_co  < p_oil);
    TEST_ASSERT_TRUE(p_oil < p_cht);
}

// --- Event name formatting ---------------------------------------

void test_all_event_types_format(void) {
    ring_log_alarm(LOG_ALARM_ASSERT,       find_alarm("CO_DETECT"),    100);
    ring_log_alarm(LOG_ALARM_ACK,          find_alarm("CO_DETECT"),    200);
    ring_log_alarm(LOG_ALARM_CLEAR,        find_alarm("CO_DETECT"),    300);
    ring_log_alarm(LOG_ALARM_CLEAR_NO_ACK, find_alarm("CO_DETECT"),    400);

    for (int i = 0; i < 4; i++) ring_log_tick();

    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "ASSERT"));
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "ACK"));
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "CLEAR"));
    TEST_ASSERT_NOT_NULL(strstr(hal_log_buf, "CLEAR_NO_ACK"));
}

// --- Drop on full --------------------------------------------------

void test_overflow_drops_with_counter(void) {
    // Capacity 32 — circular buffer with one slot reserved for full-detection
    // means 31 usable slots before drops start.
    for (uint32_t i = 0; i < 31; i++) {
        ring_log_alarm(LOG_ALARM_ASSERT, 0, i);
    }
    TEST_ASSERT_EQUAL(0, ring_log_dropped_count());

    // Two more should drop.
    ring_log_alarm(LOG_ALARM_ASSERT, 0, 1000);
    ring_log_alarm(LOG_ALARM_ASSERT, 0, 1001);
    TEST_ASSERT_EQUAL(2, ring_log_dropped_count());
}

void test_drain_then_refill_does_not_drop(void) {
    for (uint32_t i = 0; i < 31; i++) ring_log_alarm(LOG_ALARM_ASSERT, 0, i);
    for (int    i = 0; i < 31; i++) ring_log_tick();
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());

    // Now refill — slots are free again.
    for (uint32_t i = 0; i < 31; i++) ring_log_alarm(LOG_ALARM_CLEAR, 0, 100 + i);
    TEST_ASSERT_EQUAL(0, ring_log_dropped_count());
    TEST_ASSERT_EQUAL(31, ring_log_pending_count());
}

// --- Wrap behavior ------------------------------------------------

void test_buffer_wraps_correctly(void) {
    // Fill, drain partway, refill — exercises the head/tail wrap.
    for (uint32_t i = 0; i < 20; i++) ring_log_alarm(LOG_ALARM_ASSERT, 0, i);
    for (int     i = 0; i < 15; i++) ring_log_tick();
    TEST_ASSERT_EQUAL(5, ring_log_pending_count());

    // Add 25 more — total 30, still within capacity, no drops.
    for (uint32_t i = 100; i < 125; i++) ring_log_alarm(LOG_ALARM_CLEAR, 0, i);
    TEST_ASSERT_EQUAL(0, ring_log_dropped_count());
    TEST_ASSERT_EQUAL(30, ring_log_pending_count());

    // Drain everything cleanly.
    for (int i = 0; i < 30; i++) ring_log_tick();
    TEST_ASSERT_EQUAL(0, ring_log_pending_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_empty);
    RUN_TEST(test_single_event_drains);
    RUN_TEST(test_drains_one_per_tick);
    RUN_TEST(test_event_ordering_preserved);
    RUN_TEST(test_all_event_types_format);
    RUN_TEST(test_overflow_drops_with_counter);
    RUN_TEST(test_drain_then_refill_does_not_drop);
    RUN_TEST(test_buffer_wraps_correctly);
    return UNITY_END();
}
