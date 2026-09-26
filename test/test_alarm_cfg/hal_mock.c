// test/test_alarm_cfg/hal_mock.c
#include "hal.h"
#include <string.h>
#include <stddef.h>

static const char *g_file_content = NULL;

void hal_mock_reset(void)                    { g_file_content = NULL; }
void hal_mock_set_file(const char *content)  { g_file_content = content; }

bool hal_sd_read_file(const char *path, char *buf, size_t max_len, size_t *out_len) {
    (void)path;
    if (!g_file_content) return false;
    size_t n = strlen(g_file_content);
    if (n >= max_len) return false;
    memcpy(buf, g_file_content, n + 1);
    if (out_len) *out_len = n;
    return true;
}

// Unused HAL stubs required for linking
void              hal_init(void)                                                          {}
uint32_t          hal_millis(void)                                                        { return 0; }
void              hal_alarm_configure(uint8_t ch, bool ah, hal_pull_t pull)              { (void)ch; (void)ah; (void)pull; }
bool              hal_read_alarm(uint8_t ch)                                              { (void)ch; return false; }
bool              hal_read_button(void)                                                   { return false; }
uint16_t          hal_adc_read(hal_adc_ch_t ch)                                          { (void)ch; return 0; }
void              hal_set_led_duty(hal_led_t led, uint16_t duty)                         { (void)led; (void)duty; }
void              hal_audio_play(uint8_t id)                                              { (void)id; }
bool              hal_audio_busy(void)                                                    { return false; }
bool              hal_audio_sd_ok(void)                                                   { return false; }
void              hal_pixels_write(const uint8_t *b, uint16_t n)                         { (void)b; (void)n; }
void              hal_lbuck_enable(bool en)                                               { (void)en; }
bool              hal_lbuck_pg(void)                                                      { return true; }
uint8_t           hal_eeprom_get(uint16_t addr)                                           { (void)addr; return 0; }
void              hal_eeprom_put(uint16_t addr, uint8_t val)                             { (void)addr; (void)val; }
void              hal_log_write(const uint8_t *b, size_t n)                              { (void)b; (void)n; }
void              hal_watchdog_enable(uint32_t ms)                                        { (void)ms; }
void              hal_watchdog_kick(void)                                                 {}
hal_reset_cause_t hal_reset_cause(void)                                                   { return HAL_RESET_POR; }
