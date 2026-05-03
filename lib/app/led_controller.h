// lib/app/led_controller.h
//
// Master caution RGB LED drive logic (spec §4.2, §4.3, §4.7).
//
// Pure function: maps (severity, any_pending, dimmer_norm) at the current
// time to PWM duties for the three cathodes. The caller forwards the
// resulting led_drive_t to HAL.
//
//   color  ← max active severity:  HIGH=red, MED=amber(R+G), LOW=green
//   duty   ← max(MC_FLOOR_Q12, dimmer_norm_q12)
//   flash  ← any_pending ? 2 Hz square modulation : steady

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "types.h"

typedef struct {
    uint16_t red;     // 0..4095 PWM duty (cathode-sink, so duty = brightness)
    uint16_t green;
    uint16_t blue;    // reserved for future PWM color mixing; always 0 today
} led_drive_t;

void led_controller_init(void);

void led_controller_tick(uint32_t     now_ms,
                         severity_t   severity,
                         bool         any_pending,
                         uint16_t     dimmer_norm_q12,
                         led_drive_t *out);

#endif // LED_CONTROLLER_H
