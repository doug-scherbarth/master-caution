// lib/app/dimmer.h
//
// Panel dimmer pot tracking (spec §2.2, §6.4, §4.7).
//
// Inputs raw 12-bit ADC samples, IIR-filters, normalizes to Q12 (0..4095),
// and applies the §4.7 software fail-safe (50% if raw stuck at 0 for >5s).
//
// The module does NOT call HAL — the caller passes raw ADC values to
// dimmer_tick(). This keeps the unit under test pure and matches the
// alarm_engine/debouncer pattern.

#ifndef DIMMER_H
#define DIMMER_H

#include "types.h"

void dimmer_init(void);

// Process one ADC sample. Internally rate-limited to ~50 Hz; calls within
// the rate-limit window return without updating state. raw is the 12-bit
// ADC reading (0..4095).
void dimmer_tick(uint32_t now_ms, uint16_t raw);

// Normalized brightness in Q12 (0 = full dim, 4095 = full bright).
// Returns 2048 (50%) when the soft fail-safe is active.
uint16_t dimmer_get_norm_q12(void);

// True if raw has been stuck near 0 long enough to trigger the §4.7
// soft fail-safe. The hardware pull-up fail-safe (broken wire → ~72%)
// is invisible to firmware — it just looks like a normal reading.
bool dimmer_failsafe_active(void);

#endif // DIMMER_H
