// lib/hal/hal.h
//
// Hardware abstraction layer for the master caution annunciator.
//
// Alarm inputs are returned in POSITIVE LOGIC: hal_read_alarm() applies the
// polarity configured by hal_alarm_configure() so callers always see
// true = alarm asserted.  The button is active-low and IS inverted by
// hal_read_button().
//
// Two implementations:
//   src/hal_teensy/  — real, talks to Teensy 4.1 + PCM5102 + SD card
//   src/hal_host/    — desktop mock, reads from a test fixture

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_LED_RED   = 0,
    HAL_LED_GREEN = 1,
    HAL_LED_BLUE  = 2,
    HAL_LED_AUX   = 3,   // reserved; not wired on v1.4 board (future pixel_lighting)
} hal_led_t;

typedef enum {
    HAL_ADC_DIM_IN    = 0,   // panel dimmer wiper — DB-15 #1 pin 15 → Teensy A12 (pin 26)
    HAL_ADC_BUS_SENSE = 1,   // bus voltage sense — identical divider → Teensy A13 (pin 27)
} hal_adc_ch_t;

// Input pull resistor mode for alarm channels.
typedef enum {
    HAL_PULL_DOWN = 0,   // INPUT_PULLDOWN (default; matches active-high wiring)
    HAL_PULL_UP   = 1,   // INPUT_PULLUP   (use with active-low inputs)
    HAL_PULL_NONE = 2,   // INPUT (floating — only when external pull is fitted)
} hal_pull_t;

// Reset cause reported at boot.  Read once immediately after hal_init().
typedef enum {
    HAL_RESET_POR      = 0,  // power-on or cold reset
    HAL_RESET_WATCHDOG = 1,  // hardware watchdog timeout
    HAL_RESET_OTHER    = 2,  // software reset, JTAG, lockup, etc.
} hal_reset_cause_t;

// Lifecycle
void     hal_init(void);
uint32_t hal_millis(void);

// Alarm input configuration — call for each channel after parsing alarm config.
// Sets pull resistor mode and records polarity for hal_read_alarm().
// Must be called before the first hal_read_alarm(); safe to call in hal_init()
// with defaults if config is not yet loaded.
void     hal_alarm_configure(uint8_t ch, bool active_high, hal_pull_t pull);

// Inputs (polarity-corrected: true = alarm asserted, per hal_alarm_configure)
bool     hal_read_alarm(uint8_t channel);   // 0..13
bool     hal_read_button(void);
uint16_t hal_adc_read(hal_adc_ch_t ch);     // 0..4095, 12-bit ADC

// Outputs
void     hal_set_led_duty(hal_led_t led, uint16_t duty);  // 0..4095, 12-bit PWM
void     hal_audio_play(uint8_t wav_id);
bool     hal_audio_busy(void);
bool     hal_audio_sd_ok(void);    // true if SD card mounted and 0.WAV readable

// Pixel lighting — WS2812B chain on pin 29 via WS2812Serial
// rgb_buf: n_pixels * 3 bytes, R/G/B order, one byte per channel 0..255
void     hal_pixels_write(const uint8_t *rgb_buf, uint16_t n_pixels);

// Lighting buck — Pololu D36V50F5 on pins 30 (EN) and 31 (PG)
void     hal_lbuck_enable(bool en);
bool     hal_lbuck_pg(void);       // true = power good

// EEPROM emulation (Teensy internal)
uint8_t  hal_eeprom_get(uint16_t addr);
void     hal_eeprom_put(uint16_t addr, uint8_t val);

// SD file read — returns true on success; out_len set to bytes read (excl. NUL)
// buf is NUL-terminated on success. Fails silently if file > max_len-1 bytes.
bool     hal_sd_read_file(const char *path, char *buf, size_t max_len, size_t *out_len);

// Logging — bytes go straight to USB serial on target, stdout on host
void     hal_log_write(const uint8_t *buf, size_t n);

// Hardware watchdog — enable once at end of init, kick once at END of app_tick.
void     hal_watchdog_enable(uint32_t timeout_ms);
void     hal_watchdog_kick(void);

// Reset cause — valid immediately after hal_init(), read once at boot.
hal_reset_cause_t hal_reset_cause(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_H
