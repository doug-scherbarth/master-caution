// lib/app/dimmer_gesture.h
// Detects a quick dip-and-return or bump-and-return on the dimmer knob.
// Thresholds and timeout are runtime parameters loaded from LIGHTS.CFG
// so they can be tuned without recompiling.

#ifndef DIMMER_GESTURE_H
#define DIMMER_GESTURE_H

#include <stdint.h>
#include <stdbool.h>

// Initialise with threshold percentages (0-100) and timeout in ms.
// Typical: low_pct=20, high_pct=80, timeout_ms=2000.
void dimmer_gesture_init(uint8_t low_pct, uint8_t high_pct, uint32_t timeout_ms);

// Returns true the frame a completed gesture is detected.
// A "dip" gesture  : ratio crossed below low_pct then returned above it.
// A "bump" gesture : ratio crossed above high_pct then returned below it.
// Both map to the same event (advance to next lighting config).
bool dimmer_gesture_tick(uint32_t now_ms, uint8_t ratio);

#endif // DIMMER_GESTURE_H
