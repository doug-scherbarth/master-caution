// lib/app/test_mode.h
//
// Hardware self-test sequence: RED → GREEN → BLUE, then three ascending tones.
// Triggered by a 3-second long press. Drives LEDs and audio directly via HAL,
// bypassing the normal led_controller and audio_queue.

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <stdint.h>
#include <stdbool.h>

void test_mode_init(void);
bool test_mode_active(void);

// Start the sequence. No-op if already running.
void test_mode_trigger(uint32_t now_ms);

// Cooperative tick. Call every app_tick while test_mode_active().
void test_mode_tick(uint32_t now_ms);

#endif // TEST_MODE_H
