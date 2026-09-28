// lib/app/dimmer.h
//
// Panel dimmer tracking (spec v1.4 §6.3).
//
// Accepts raw 12-bit ADC samples for DIM_IN and BUS_SENSE, computes the
// ratiometric Q12 value (dim_raw * 4096 / bus_raw), IIR-filters it, and
// normalises to Q12 brightness with end dead bands.
//
// The module does NOT call HAL — the caller passes raw ADC values so the
// unit under test remains pure.

#ifndef DIMMER_H
#define DIMMER_H

#include "types.h"

void dimmer_init(void);

// Process one pair of ADC samples (50 Hz rate-limited internally).
// dim_raw:  DIM_IN channel,  0..4095.
// bus_raw:  BUS_SENSE channel, 0..4095. If 0 the ratio is treated as 0.
void dimmer_tick(uint32_t now_ms, uint16_t dim_raw, uint16_t bus_raw);

// Normalised brightness in Q12 (0 = full dim, 4095 = full bright).
// Returns 0 when ratio is below the low dead band.
uint16_t dimmer_get_norm_q12(void);

#endif // DIMMER_H
