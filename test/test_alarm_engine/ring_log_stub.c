// test/test_alarm_engine/ring_log_stub.c
//
// Silent stub. Tests don't currently assert log behavior; we just need
// the symbols to link.

#include "ring_log.h"

void ring_log_init(void) {}
void ring_log_alarm(log_event_t e, uint8_t i, uint32_t ms) {
    (void)e; (void)i; (void)ms;
}
void ring_log_tick(void) {}
