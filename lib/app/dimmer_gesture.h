// lib/app/dimmer_gesture.h
// Detects a quick dip-and-return or bump-and-return on the dimmer knob,
// using two thresholds so a full-scale sweep is never required.
//
// Call dimmer_gesture_tick() each frame with the current ratio (0-255).
// It returns true exactly once per completed gesture.
//
// Thresholds are compile-time constants; tune for your knob travel.

#ifndef DIMMER_GESTURE_H
#define DIMMER_GESTURE_H

#include <stdint.h>
#include <stdbool.h>

// Ratio (0-255) below which the knob is considered "dim"
#define DIM_GESTURE_LOW_THRESH    51u   // ~20 %

// Ratio (0-255) above which the knob is considered "bright"
#define DIM_GESTURE_HIGH_THRESH  204u   // ~80 %

// Max milliseconds between entering a zone and leaving it.
// A dwell longer than this is treated as intentional setting, not a gesture.
#define DIM_GESTURE_TIMEOUT_MS  2000u

void dimmer_gesture_init(void);

// Returns true the frame a completed gesture is detected.
// A "dip" gesture  : ratio crossed below LOW  then returned above LOW.
// A "bump" gesture : ratio crossed above HIGH  then returned below HIGH.
// Both map to the same event (advance to next lighting config).
bool dimmer_gesture_tick(uint32_t now_ms, uint8_t ratio);

#endif // DIMMER_GESTURE_H
