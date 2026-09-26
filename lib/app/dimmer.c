// lib/app/dimmer.c
#include "dimmer.h"
#include <string.h>

// Spec v1.4 §6.3 tunables (match gesture_cfg.h defaults).
#define SAMPLE_PERIOD_MS          20u    // 50 Hz
#define DIM_DEADBAND_LOW         123u    // 3%  * 4095
#define DIM_DEADBAND_HIGH       3972u    // 97% * 4095
#define FAILSAFE_RATIO_THRESHOLD   5u    // ratio counts; "stuck at zero" noise margin
#define FAILSAFE_HOLDOFF_MS     5000u

typedef struct {
    bool     initialized;
    uint16_t filtered_ratio;       // IIR-smoothed Q12 ratio (0..4095)
    uint32_t last_sample_ms;
    bool     zero_holdoff_active;
    uint32_t zero_started_ms;
    bool     failsafe_active;
} dimmer_state_t;

static dimmer_state_t g_d;

void dimmer_init(void) {
    memset(&g_d, 0, sizeof(g_d));
}

void dimmer_tick(uint32_t now_ms, uint16_t dim_raw, uint16_t bus_raw) {
    if (g_d.initialized &&
        (uint32_t)(now_ms - g_d.last_sample_ms) < SAMPLE_PERIOD_MS) {
        return;
    }
    g_d.initialized    = true;
    g_d.last_sample_ms = now_ms;

    // Ratiometric: ratio_q12 = dim_raw / bus_raw, scaled to 0..4095.
    uint16_t ratio_q12;
    if (bus_raw == 0u) {
        ratio_q12 = 0u;
    } else {
        uint32_t r = (uint32_t)dim_raw * 4096u / bus_raw;
        ratio_q12  = (r > 4095u) ? 4095u : (uint16_t)r;
    }

    // IIR α = 1/4: time constant ≈ 3–4 samples ≈ 70 ms at 50 Hz.
    g_d.filtered_ratio =
        (uint16_t)(((uint32_t)g_d.filtered_ratio * 3u + ratio_q12 + 2u) / 4u);

    // Soft fail-safe: ratio stuck near 0 for > 5 s → hold at 50%.
    if (ratio_q12 < FAILSAFE_RATIO_THRESHOLD) {
        if (!g_d.zero_holdoff_active) {
            g_d.zero_holdoff_active = true;
            g_d.zero_started_ms     = now_ms;
        } else if ((uint32_t)(now_ms - g_d.zero_started_ms) > FAILSAFE_HOLDOFF_MS) {
            g_d.failsafe_active = true;
        }
    } else {
        g_d.zero_holdoff_active = false;
        g_d.failsafe_active     = false;
    }
}

uint16_t dimmer_get_norm_q12(void) {
    if (g_d.failsafe_active) return 2048u;

    const int32_t f = (int32_t)g_d.filtered_ratio;
    if (f <= (int32_t)DIM_DEADBAND_LOW)  return 0u;
    if (f >= (int32_t)DIM_DEADBAND_HIGH) return 4095u;

    int32_t scaled = (f - (int32_t)DIM_DEADBAND_LOW) * 4095
                   / ((int32_t)DIM_DEADBAND_HIGH - (int32_t)DIM_DEADBAND_LOW);
    if (scaled < 0)    scaled = 0;
    if (scaled > 4095) scaled = 4095;
    return (uint16_t)scaled;
}

bool dimmer_failsafe_active(void) {
    return g_d.failsafe_active;
}
