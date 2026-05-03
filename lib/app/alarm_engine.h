// lib/app/alarm_engine.h
//
// Per-alarm state machine + global rollups.
//
// State machine (per alarm):
//
//      condition asserted
//   INACTIVE ───────────────► PENDING_ACK
//      ▲                          │   │
//      │ condition cleared        │   │ button press
//      │                          ▼   ▼
//      │                       (clr) ACKNOWLEDGED
//      └──────────────────────────────────┘
//                  condition cleared
//
// Side effects:
//   INACTIVE → PENDING_ACK  : audio_queue_push(wav_id, severity)
//   PENDING_ACK → ACKNOWLEDGED : (display logic only; no audio repeat)
//   * → INACTIVE            : (none; logging only)
//
// Re-trigger (spec §4.6) is implicit: an ACKNOWLEDGED alarm whose condition
// clears returns to INACTIVE. The next assertion is just another normal
// INACTIVE → PENDING_ACK transition, which fires audio again.

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include "types.h"

typedef enum {
    AS_INACTIVE = 0,
    AS_PENDING_ACK,
    AS_ACKNOWLEDGED,
} alarm_state_t;

typedef struct {
    alarm_state_t state;
    uint32_t      entered_state_ms;
} alarm_runtime_t;

void alarm_engine_init(void);

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
