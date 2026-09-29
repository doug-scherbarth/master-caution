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

void hal_audio_set_gain(float gain)                { (void)gain; }
bool hal_audio_busy(void)                          { return g_busy; }
void hal_init(void)                                {}
uint32_t hal_millis(void)                          { return 0; }
bool hal_read_alarm(uint8_t ch)                    { (void)ch; return false; }
bool hal_read_button(void)                         { return false; }
uint16_t hal_adc_read(hal_adc_ch_t ch)                                       { (void)ch; return 0; }
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
