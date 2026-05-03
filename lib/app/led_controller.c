// lib/app/led_controller.c
#include "led_controller.h"
#include <string.h>

// 15% of 4095 = 614 (spec §4.7 MC_FLOOR)
#define MC_FLOOR_Q12      614u

// 2 Hz flash: 250 ms ON, 250 ms OFF (spec §4.2)
#define FLASH_HALF_MS     250u

void led_controller_init(void) {
    // Stateless module; init exists for symmetry and future use.
}

void led_controller_tick(uint32_t     now_ms,
                         severity_t   severity,
                         bool         any_pending,
                         uint16_t     dimmer_norm_q12,
                         led_drive_t *out) {
    memset(out, 0, sizeof(*out));

    if (severity == SEV_NONE) return;

    // Brightness duty: dimmer-driven, with 15% floor for visibility.
    uint16_t lit_duty = (dimmer_norm_q12 < MC_FLOOR_Q12)
                          ? MC_FLOOR_Q12
                          : dimmer_norm_q12;

    // Flash modulation: ON during first half of each 500 ms cycle.
    if (any_pending) {
        const bool on_phase = (((uint32_t)(now_ms / FLASH_HALF_MS)) & 1u) == 0u;
        if (!on_phase) lit_duty = 0;
    }

    // Severity → which cathodes get the lit duty.
    switch (severity) {
    case SEV_HIGH:  out->red = lit_duty;                          break;
    case SEV_MED:   out->red = lit_duty;  out->green = lit_duty;  break;
    case SEV_LOW:                          out->green = lit_duty; break;
    default:        break;  // unreachable; SEV_NONE handled above
    }
    // out->blue stays 0 — reserved for future color expansion (spec §4.3).
}
