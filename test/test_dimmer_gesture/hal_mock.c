// test/test_dimmer_gesture/hal_mock.c
// dimmer_gesture is pure logic — no HAL calls. Minimal stubs for the linker.

#include "hal.h"
#include <stddef.h>

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
hal_sd_status_t hal_sd_read_file(const char *p, char *b, size_t m, size_t *ol) { (void)p; (void)b; (void)m; (void)ol; return HAL_SD_NOT_FOUND; }

void hal_watchdog_enable(uint32_t ms) { (void)ms; }
void hal_watchdog_kick(void) {}
void hal_alarm_configure(uint8_t ch, bool ah, hal_pull_t pull) { (void)ch; (void)ah; (void)pull; }
hal_reset_cause_t hal_reset_cause(void) { return HAL_RESET_POR; }
