// lib/app/debouncer.h
//
// Per-channel qualification timer. Raw input must remain in its new state
// for CHANNEL_TABLE[i].debounce_ms before the debounced output transitions.
// A glitch back to the original state during qualification cancels the
// transition cleanly.

#ifndef DEBOUNCER_H
#define DEBOUNCER_H

#include "types.h"

void debouncer_init(void);

// raw and out are CHANNEL_COUNT-length arrays of polarity-corrected booleans.
// Both pointers may NOT be NULL. raw and out may NOT alias.
void debouncer_tick(uint32_t now_ms, const bool *raw, bool *out);

#endif // DEBOUNCER_H
