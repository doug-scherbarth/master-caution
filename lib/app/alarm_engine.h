// lib/app/alarm_engine.h
//
// Per-alarm state machine + global rollups.
//
// State machine (per alarm):
//
//        condition asserted
//   INACTIVE ─────────────────► PENDING_ACK ──────────────┐
//      ▲  ▲                     (flashing)   condition     │ button
//      │  │                          │        clears       │ press
//      │  │                          ▼                     ▼
//      │  │                   PENDING_ACK_CLEARED      ACKNOWLEDGED
//      │  │                     (flashing)               (steady)
//      │  │                          │                     │
//      │  └──────────────────────────┘                     │
//      │          button press                             │
//      └───────────────────────────────────────────────────┘
//                           condition cleared
//
// An unacknowledged alarm always latches — PENDING_ACK_CLEARED keeps the
// button flashing even after the triggering condition goes away, so the pilot
// must consciously acknowledge every onset regardless of when it clears.
//
// Side effects:
//   INACTIVE → PENDING_ACK         : audio_queue_push(wav_id, severity)
//   PENDING_ACK_CLEARED → PENDING_ACK : audio_queue_push (new onset, re-asserted)
//   PENDING_ACK → ACKNOWLEDGED     : (display logic only; no audio repeat)
//   PENDING_ACK_CLEARED → INACTIVE : (button press; logging only)
//   ACKNOWLEDGED → INACTIVE        : (condition cleared; logging only)
//
// Re-trigger (spec §4.6): a cleared-and-acked alarm returns to INACTIVE;
// the next assertion is a fresh INACTIVE → PENDING_ACK transition with audio.

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include "types.h"

typedef enum {
    AS_INACTIVE = 0,
    AS_PENDING_ACK,
    AS_PENDING_ACK_CLEARED,  // input gone before ack; still flashing until button press
    AS_ACKNOWLEDGED,
} alarm_state_t;

typedef struct {
    alarm_state_t state;
    uint32_t      entered_state_ms;
} alarm_runtime_t;

void alarm_engine_init(void);

// Override the WAV file played for one alarm (spec §2.3).
// wav_id must be in [0, WAV_COUNT-1]; out-of-range calls are silently ignored.
// 0xFF (the initial value) means use the compiled ALARM_TABLE default.
// Call between alarm_engine_init() and the first tick.
void alarm_engine_set_wav_override(uint8_t alarm_idx, uint8_t wav_id);

// Called every tick. `channel_state` is a CHANNEL_COUNT-length array of
// post-debounce booleans (true = asserted, polarity already corrected).
void alarm_engine_tick(uint32_t now_ms, const bool *channel_state);

// Called once when the master caution button transitions to pressed.
// Promotes every PENDING_ACK alarm to ACKNOWLEDGED in one pass.
void alarm_engine_on_button_press(uint32_t now_ms);

// Rollups consumed by led_controller.
severity_t alarm_engine_max_active_severity(void);  // SEV_NONE if none active
bool       alarm_engine_any_pending_ack(void);

// Introspection — for tests and serial logging.
const alarm_runtime_t *alarm_engine_runtime(uint8_t alarm_idx);

#endif // ALARM_ENGINE_H
