// lib/hal/hal.h
//
// Hardware abstraction layer for the master caution annunciator.
//
// All inputs are returned in POSITIVE LOGIC: the underlying signals are
// active-low at the connector, but hal_read_alarm()/hal_read_button()
// invert before returning. Application code never deals with polarity.
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
    HAL_LED_AUX   = 3,
} hal_led_t;

// Lifecycle
void     hal_init(void);
uint32_t hal_millis(void);

// Inputs (polarity-corrected: true = asserted)
bool     hal_read_alarm(uint8_t channel);   // 0..14
bool     hal_read_button(void);
uint16_t hal_read_dimmer_raw(void);          // 0..4095, 12-bit ADC

// Outputs
void     hal_set_led_duty(hal_led_t led, uint16_t duty);  // 0..4095, 12-bit PWM
void     hal_audio_play(uint8_t wav_id);
bool     hal_audio_busy(void);

// Logging — bytes go straight to USB serial on target, stdout on host
void     hal_log_write(const uint8_t *buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif // HAL_H
