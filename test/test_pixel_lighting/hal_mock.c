// test/test_pixel_lighting/hal_mock.c
// HAL mock for pixel_lighting tests.
// Records hal_pixels_write() calls and allows ADC values to be set.

#include "hal.h"
#include <string.h>
#include <stddef.h>

// --- pixels_write recording ---
#define MOCK_PIX_MAX 150
static uint8_t  g_last_pixels[MOCK_PIX_MAX * 3];
static uint16_t g_last_n_pixels = 0;
static int      g_pixels_write_calls = 0;

// --- ADC mock ---
static uint16_t g_adc[2];  // [0]=DIM_IN, [1]=BUS_SENSE

// --- SD file mock ---
static const char *g_sd_content = NULL;

// Test-side controls
void hal_mock_reset(void) {
    memset(g_last_pixels, 0, sizeof(g_last_pixels));
    g_last_n_pixels      = 0;
    g_pixels_write_calls = 0;
    g_adc[0]             = 0;
    g_adc[1]             = 4095;  // default BUS_SENSE = full-scale to avoid /0
    g_sd_content         = NULL;
}

void hal_mock_set_adc(hal_adc_ch_t ch, uint16_t val) {
    if ((int)ch < 2) g_adc[(int)ch] = val;
}

// Expose recorded pixel data to tests
const uint8_t *hal_mock_get_pixels(void)        { return g_last_pixels; }
uint16_t       hal_mock_get_n_pixels(void)      { return g_last_n_pixels; }
int            hal_mock_pixels_write_count(void){ return g_pixels_write_calls; }

// HAL implementation
void hal_pixels_write(const uint8_t *buf, uint16_t n) {
    g_pixels_write_calls++;
    if (n > MOCK_PIX_MAX) n = MOCK_PIX_MAX;
    g_last_n_pixels = n;
    if (n > 0) memcpy(g_last_pixels, buf, (size_t)n * 3u);
}

uint16_t hal_adc_read(hal_adc_ch_t ch) {
    if ((int)ch < 2) return g_adc[(int)ch];
    return 0;
}

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
void     hal_set_led_duty(hal_led_t l, uint16_t d)                          { (void)l; (void)d; }
void     hal_log_write(const uint8_t *b, size_t n)                          { (void)b; (void)n; }
void     hal_audio_play(uint8_t w)                                           { (void)w; }
void              hal_audio_set_gain(float g)                                                   { (void)g; }
bool     hal_audio_busy(void)                                                { return false; }
bool     hal_audio_sd_ok(void)                                               { return true; }
void     hal_lbuck_enable(bool en)                                           { (void)en; }
bool     hal_lbuck_pg(void)                                                  { return true; }
uint8_t  hal_eeprom_get(uint16_t addr)                                       { (void)addr; return 0; }
void     hal_eeprom_put(uint16_t addr, uint8_t val)                          { (void)addr; (void)val; }

void hal_watchdog_enable(uint32_t ms) { (void)ms; }
void hal_watchdog_kick(void) {}
void hal_alarm_configure(uint8_t ch, bool ah, hal_pull_t pull) { (void)ch; (void)ah; (void)pull; }
hal_reset_cause_t hal_reset_cause(void) { return HAL_RESET_POR; }
