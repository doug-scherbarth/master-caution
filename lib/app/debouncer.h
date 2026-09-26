// lib/app/debouncer.h
//
// Per-channel qualification timer. Raw input must remain in its new state
// for the per-channel debounce_ms before the debounced output transitions.
// A glitch back to the original state during qualification cancels the
// transition cleanly.

#ifndef DEBOUNCER_H
#define DEBOUNCER_H

#include "types.h"

// debounce_ms must point to a CHANNEL_COUNT-length array that remains valid
// for the life of the module (values are copied into internal storage).
void debouncer_init(const uint16_t *debounce_ms);

// raw and out are CHANNEL_COUNT-length arrays of polarity-corrected booleans.
// Both pointers may NOT be NULL. raw and out may NOT alias.
void debouncer_tick(uint32_t now_ms, const bool *raw, bool *out);

#endif // DEBOUNCER_H
