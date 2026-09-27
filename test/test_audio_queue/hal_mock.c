// test/test_audio_queue/hal_mock.c
//
// Minimal HAL mock for audio_queue tests. Records hal_audio_play() calls
// and lets the test set hal_audio_busy() at will. Only the symbols the
// audio_queue module touches are implemented.

#include "hal.h"
#include <stddef.h>

// Recorded state ----------------------------------------------------
int     hal_play_calls          = 0;
uint8_t hal_play_last_wav       = 0xFF;

#define HAL_PLAY_LOG_SIZE 32
uint8_t hal_play_log[HAL_PLAY_LOG_SIZE];
int     hal_play_log_count      = 0;

static bool g_busy = false;

// Test-side controls ------------------------------------------------
void hal_mock_reset(void) {
    hal_play_calls     = 0;
    hal_play_last_wav  = 0xFF;
    hal_play_log_count = 0;
    g_busy             = false;
}

void hal_mock_set_busy(bool busy) { g_busy = busy; }

// HAL implementation ------------------------------------------------
void hal_audio_play(uint8_t wav_id) {
    hal_play_calls++;
    hal_play_last_wav = wav_id;
    if (hal_play_log_count < HAL_PLAY_LOG_SIZE) {
        hal_play_log[hal_play_log_count++] = wav_id;
    }
    g_busy = true;   // playback starts → HAL becomes busy
}

bool hal_audio_busy(void) { return g_busy; }
bool hal_audio_sd_ok(void) { return true; }

// Stubs for everything else the linker may pull in. None are used by
// audio_queue, but keep them here so the mock is self-contained.
void     hal_init(void)                                {}
uint32_t hal_millis(void)                              { return 0; }
bool     hal_read_alarm(uint8_t ch)                    { (void)ch; return false; }
bool     hal_read_button(void)                         { return false; }
uint16_t hal_adc_read(hal_adc_ch_t ch)                                       { (void)ch; return 0; }
void     hal_set_led_duty(hal_led_t l, uint16_t d)                          { (void)l; (void)d; }
void     hal_log_write(const uint8_t *b, size_t n)                          { (void)b; (void)n; }
void     hal_pixels_write(const uint8_t *b, uint16_t n)                     { (void)b; (void)n; }
void     hal_lbuck_enable(bool en)                                           { (void)en; }
bool     hal_lbuck_pg(void)                                                  { return true; }
uint8_t  hal_eeprom_get(uint16_t addr)                                       { (void)addr; return 0; }
void     hal_eeprom_put(uint16_t addr, uint8_t val)                          { (void)addr; (void)val; }
hal_sd_status_t hal_sd_read_file(const char *p, char *b, size_t m, size_t *ol) { (void)p; (void)b; (void)m; (void)ol; return HAL_SD_NOT_FOUND; }

void hal_watchdog_enable(uint32_t ms) { (void)ms; }
void hal_watchdog_kick(void) {}
void hal_alarm_configure(uint8_t ch, bool ah, hal_pull_t pull) { (void)ch; (void)ah; (void)pull; }
hal_reset_cause_t hal_reset_cause(void) { return HAL_RESET_POR; }
