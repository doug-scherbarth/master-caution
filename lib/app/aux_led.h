// lib/app/aux_led.h
//
// Auxiliary panel LED PWM follower (spec one-pager §"Auxiliary panel LED PWM").
//
// Pure function: returns the dimmer-normalized duty unchanged. No MC_FLOOR
// applied — fully dim means panel LEDs off completely. Returned value
// feeds hal_set_led_duty(HAL_LED_AUX, ...).

#ifndef AUX_LED_H
#define AUX_LED_H

#include "types.h"

void aux_led_init(void);

// Maps the dimmer reading to PWM duty for the aux LED string.
// Returns Q12 (0..4095). Caller forwards to HAL.
uint16_t aux_led_compute_duty(uint16_t dimmer_norm_q12);

#endif // AUX_LED_H
