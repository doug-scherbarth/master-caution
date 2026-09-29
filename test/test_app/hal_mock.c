// test/test_app/hal_mock.c
// Full, time-aware HAL mock for app-level integration tests.
// Uses the tri-state hal_sd_read_file() from TASKS.md Task 1.
#include "hal.h"
#include "channel_table.h"
#include <string.h>

#define PLAY_LOG_MAX  64
#define AUDIO_LEN_MS 300u

uint32_t g_mock_now;
uint16_t mock_led_duty[4];
uint8_t  mock_play_log[PLAY_LOG_MAX];
int      mock_play_count;
int      mock_eeprom_puts;

static bool              s_alarm[CHANNEL_COUNT];
static bool              s_button;
static uint16_t          s_adc[2];
static uint32_t          s_audio_until;
static bool              s_audio_active;
static hal_reset_cause_t s_reset;
static const char       *s_lights_cfg;
static uint8_t           s_eeprom[16];

void hal_mock_reset(void) {
    g_mock_now = 0;
    memset(mock_led_duty, 0, sizeof(mock_led_duty));
    mock_play_count = 0;
    mock_eeprom_puts = 0;
    memset(s_alarm, 0, sizeof(s_alarm));
    s_button = false;
    s_adc[HAL_ADC_DIM_IN]    = 1500u;
    s_adc[HAL_ADC_BUS_SENSE] = 3039u;   // ~12 V
    s_audio_active = false;
    s_reset = HAL_RESET_POR;
    s_lights_cfg = NULL;
    memset(s_eeprom, 0, sizeof(s_eeprom));
}
void hal_mock_set_alarm(uint8_t ch, bool v)             { if (ch < CHANNEL_COUNT) s_alarm[ch] = v; }
void hal_mock_set_button(bool pressed)                  { s_button = pressed; }
void hal_mock_set_adc(hal_adc_ch_t ch, uint16_t v)      { if (ch < 2) s_adc[ch] = v; }
void hal_mock_set_reset_cause(hal_reset_cause_t c)      { s_reset = c; }
void hal_mock_set_lights_cfg(const char *text)          { s_lights_cfg = text; }

bool mock_played(uint8_t wav) {
    for (int i = 0; i < mock_play_count && i < PLAY_LOG_MAX; i++)
        if (mock_play_log[i] == wav) return true;
    return false;
}

void              hal_init(void)                         {}
uint32_t          hal_millis(void)                       { return g_mock_now; }
hal_reset_cause_t hal_reset_cause(void)                  { return s_reset; }
void              hal_alarm_configure(uint8_t c, bool a, hal_pull_t p) { (void)c; (void)a; (void)p; }
bool              hal_read_alarm(uint8_t ch)             { return ch < CHANNEL_COUNT ? s_alarm[ch] : false; }
bool              hal_read_button(void)                  { return s_button; }
uint16_t          hal_adc_read(hal_adc_ch_t ch)          { return ch < 2 ? s_adc[ch] : 0u; }
void              hal_set_led_duty(hal_led_t l, uint16_t d) { if (l < 4) mock_led_duty[l] = d; }

void hal_audio_play(uint8_t wav_id) {
    if (mock_play_count < PLAY_LOG_MAX) mock_play_log[mock_play_count] = wav_id;
    mock_play_count++;
    s_audio_active = true;
    s_audio_until  = g_mock_now + AUDIO_LEN_MS;
}
void hal_audio_set_gain(float gain) { (void)gain; }
bool hal_audio_busy(void) {
    if (s_audio_active && (int32_t)(g_mock_now - s_audio_until) >= 0) s_audio_active = false;
    return s_audio_active;
}
bool hal_audio_sd_ok(void) { return true; }

hal_sd_status_t hal_sd_read_file(const char *path, char *buf, size_t max_len, size_t *out_len) {
    const char *src = NULL;
    if (strcmp(path, "/LIGHTS.CFG") == 0) src = s_lights_cfg;
    if (!src) return HAL_SD_NOT_FOUND;
    size_t n = strlen(src);
    if (n >= max_len) return HAL_SD_TOO_BIG;
    memcpy(buf, src, n + 1);
    if (out_len) *out_len = n;
    return HAL_SD_OK;
}

void    hal_pixels_write(const uint8_t *b, uint16_t n) { (void)b; (void)n; }
void    hal_lbuck_enable(bool en)                      { (void)en; }
bool    hal_lbuck_pg(void)                             { return true; }
uint8_t hal_eeprom_get(uint16_t a)                     { return a < 16 ? s_eeprom[a] : 0; }
void    hal_eeprom_put(uint16_t a, uint8_t v)          { if (a < 16) s_eeprom[a] = v; mock_eeprom_puts++; }
void    hal_log_write(const uint8_t *b, size_t n)      { (void)b; (void)n; }
void    hal_watchdog_enable(uint32_t ms)               { (void)ms; }
void    hal_watchdog_kick(void)                        {}
