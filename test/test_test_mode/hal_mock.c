// test/test_test_mode/hal_mock.c
// HAL mock for test_mode tests. Tracks LED duties and audio calls;
// lets the test control hal_audio_busy().

#include "hal.h"
#include <stddef.h>

// LED state ---------------------------------------------------------
uint16_t hal_led_duty[4];   // indexed by hal_led_t

// Audio state -------------------------------------------------------
int     hal_play_calls    = 0;
uint8_t hal_play_log[16];
int     hal_play_log_count = 0;
static bool g_busy = false;

// Test-side controls ------------------------------------------------
void hal_mock_reset(void) {
    for (int i = 0; i < 4; i++) hal_led_duty[i] = 0;
    hal_play_calls     = 0;
    hal_play_log_count = 0;
    g_busy             = false;
}

void hal_mock_set_busy(bool busy) { g_busy = busy; }

// HAL implementation ------------------------------------------------
void hal_set_led_duty(hal_led_t led, uint16_t duty) {
    hal_led_duty[led] = duty;
}

void hal_audio_play(uint8_t wav_id) {
    hal_play_calls++;
    if (hal_play_log_count < 16) hal_play_log[hal_play_log_count++] = wav_id;
    g_busy = true;
}

bool hal_audio_busy(void)                          { return g_busy; }
void hal_init(void)                                {}
uint32_t hal_millis(void)                          { return 0; }
bool hal_read_alarm(uint8_t ch)                    { (void)ch; return false; }
bool hal_read_button(void)                         { return false; }
uint16_t hal_read_dimmer_raw(void)                 { return 0; }
void hal_log_write(const uint8_t *b, size_t n)    { (void)b; (void)n; }
