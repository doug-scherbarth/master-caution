// lib/app/test_mode.h
//
// Hardware self-test sequence: RED → GREEN → BLUE, then three ascending tones.
// Triggered by a qualified button press. Drives LEDs and audio directly via
// HAL, bypassing the normal led_controller and audio_queue. Caller must hold
// off those subsystems while test_mode_active() returns true.
//
// Reserved wav IDs for the three test tones (above WAV_COUNT=13):
#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <stdint.h>
#include <stdbool.h>

#define TEST_WAV_LO   13   // 440 Hz
#define TEST_WAV_MID  14   // 880 Hz
#define TEST_WAV_HI   15   // 1320 Hz

void test_mode_init(void);
bool test_mode_active(void);

// Start the sequence. No-op if already running.
void test_mode_trigger(uint32_t now_ms);

// Cooperative tick. Call every app_tick while test_mode_active().
void test_mode_tick(uint32_t now_ms);

#endif // TEST_MODE_H
