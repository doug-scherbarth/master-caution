// test/test_light_cfg/hal_mock.c
// Minimal HAL mock for light_cfg tests.
// hal_sd_read_file is controllable via hal_mock_set_sd_file().

#include "hal.h"
#include <string.h>
#include <stddef.h>

// SD file mock state
static const char *g_sd_content = NULL;

void hal_mock_reset(void) {
    g_sd_content = NULL;
}

// Set up the mock SD content.  Pass NULL to simulate a missing file.
void hal_mock_set_sd_file(const char *content) {
    g_sd_content = content;
}

// HAL implementation
hal_sd_status_t hal_sd_read_file(const char *path, char *buf, size_t max_len, size_t *out_len) {
    (void)path;
    if (!g_sd_content) return HAL_SD_NOT_FOUND;
    size_t n = strlen(g_sd_content);
    if (n >= max_len) return HAL_SD_TOO_BIG;
    memcpy(buf, g_sd_content, n);
    buf[n] = '\0';
    if (out_len) *out_len = n;
    return HAL_SD_OK;
}

// Stubs
void     hal_init(void)                                                      {}
uint32_t hal_millis(void)                                                    { return 0; }
bool     hal_read_alarm(uint8_t ch)                                          { (void)ch; return false; }
bool     hal_read_button(void)                                               { return false; }
uint16_t hal_adc_read(hal_adc_ch_t ch)                                       { (void)ch; return 0; }
void     hal_set_led_duty(hal_led_t l, uint16_t d)                          { (void)l; (void)d; }
void     hal_log_write(const uint8_t *b, size_t n)                          { (void)b; (void)n; }
void     hal_audio_play(uint8_t w)                                           { (void)w; }
bool     hal_audio_busy(void)                                                { return false; }
bool     hal_audio_sd_ok(void)                                               { return true; }
void     hal_pixels_write(const uint8_t *b, uint16_t n)                     { (void)b; (void)n; }
void     hal_lbuck_enable(bool en)                                           { (void)en; }
bool     hal_lbuck_pg(void)                                                  { return true; }
uint8_t  hal_eeprom_get(uint16_t addr)                                       { (void)addr; return 0; }
void     hal_eeprom_put(uint16_t addr, uint8_t val)                          { (void)addr; (void)val; }

void hal_watchdog_enable(uint32_t ms) { (void)ms; }
void hal_watchdog_kick(void) {}
void hal_alarm_configure(uint8_t ch, bool ah, hal_pull_t pull) { (void)ch; (void)ah; (void)pull; }
hal_reset_cause_t hal_reset_cause(void) { return HAL_RESET_POR; }

// ring_log stub — light_cfg_load() may call ring_log_fault for TOO_BIG
void ring_log_fault(uint8_t fault_code, uint32_t now_ms) { (void)fault_code; (void)now_ms; }
