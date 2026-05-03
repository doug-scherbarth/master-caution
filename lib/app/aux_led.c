// lib/app/aux_led.c
#include "aux_led.h"

void aux_led_init(void) {
    // Stateless module; init exists for symmetry with other modules.
}

uint16_t aux_led_compute_duty(uint16_t dimmer_norm_q12) {
    return dimmer_norm_q12;
}
