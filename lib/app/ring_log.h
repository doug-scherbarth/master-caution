// lib/app/ring_log.h
//
// Timestamped event ring buffer drained to USB serial in the background.
// Used for post-flight debug and breadboard development.
//
// Design:
//   - Fixed 32-entry circular buffer, drop-on-full (preserves earliest events).
//   - Producer is alarm_engine on state transitions; consumer is the
//     scheduler-driven ring_log_tick() which formats and pushes to HAL.
//   - One record per tick maximum, so a flood doesn't monopolize the loop.

#ifndef RING_LOG_H
#define RING_LOG_H

#include "types.h"

typedef enum {
    LOG_NONE               = 0,
    LOG_ALARM_ASSERT       = 1,
    LOG_ALARM_ACK          = 2,
    LOG_ALARM_CLEAR        = 3,
    LOG_ALARM_CLEAR_NO_ACK = 4,
} log_event_t;

void ring_log_init(void);

// Producer side. Called from alarm_engine on state transitions.
// alarm_idx is the index into ALARM_TABLE; the drain side resolves to a name.
void ring_log_alarm(log_event_t event, uint8_t alarm_idx, uint32_t now_ms);

// Consumer side. Called from the cooperative scheduler. Drains at most
// one record per call, formats it, and writes via hal_log_write().
void ring_log_tick(void);

// Diagnostics
uint32_t ring_log_dropped_count(void);
uint8_t  ring_log_pending_count(void);

#endif // RING_LOG_H
