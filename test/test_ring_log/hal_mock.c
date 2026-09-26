// test/test_ring_log/hal_mock.c
//
// Captures hal_log_write output into a buffer the test can inspect.
// Only the symbols ring_log uses are real; everything else is stubbed.

#include "hal.h"
#include <string.h>

#define MOCK_LOG_BUF_SIZE  4096

char    hal_log_buf[MOCK_LOG_BUF_SIZE];
size_t  hal_log_buf_len = 0;

void hal_mock_reset(void) {
    memset(hal_log_buf, 0, sizeof(hal_log_buf));
    hal_log_buf_len = 0;
}

void hal_log_write(const uint8_t *buf, size_t n) {
    if (hal_log_buf_len + n >= MOCK_LOG_BUF_SIZE) return;   // truncate; tests stay small
    memcpy(hal_log_buf + hal_log_buf_len, buf, n);
    hal_log_buf_len += n;
    hal_log_buf[hal_log_buf_len] = '\0';
}

// Stubs --------------------------------------------------------------
void     hal_init(void)                                                      {}
uint32_t hal_millis(void)                                                    { return 0; }
bool     hal_read_alarm(uint8_t ch)                                          { (void)ch; return false; }
bool     hal_read_button(void)                                               { return false; }
uint16_t hal_adc_read(hal_adc_ch_t ch)                                       { (void)ch; return 0; }
void     hal_set_led_duty(hal_led_t l, uint16_t d)                          { (void)l; (void)d; }
void     hal_audio_play(uint8_t w)                                           { (void)w; }
bool     hal_audio_busy(void)                                                { return false; }
bool     hal_audio_sd_ok(void)                                               { return true; }
void     hal_pixels_write(const uint8_t *b, uint16_t n)                     { (void)b; (void)n; }
void     hal_lbuck_enable(bool en)                                           { (void)en; }
bool     hal_lbuck_pg(void)                                                  { return true; }
uint8_t  hal_eeprom_get(uint16_t addr)                                       { (void)addr; return 0; }
void     hal_eeprom_put(uint16_t addr, uint8_t val)                          { (void)addr; (void)val; }
bool     hal_sd_read_file(const char *p, char *b, size_t m, size_t *ol)     { (void)p; (void)b; (void)m; (void)ol; return false; }

void hal_watchdog_enable(uint32_t ms) { (void)ms; }
void hal_watchdog_kick(void) {}
