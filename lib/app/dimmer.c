// lib/app/dimmer.c
#include "dimmer.h"
#include <string.h>

// --- Calibration ---------------------------------------------------------
// These are bench-tunable per spec §9 open item:
//   "Bench-measure panel pot value and end-of-travel voltages to confirm
//    divider/pull-up trim."
//
// At full bright (12V wiper), divider gives 2.98V at ADC ≈ raw 3700.
// At full dim   (0V wiper), divider gives 0V at ADC ≈ raw 0 (with offset).
// At broken wiper (hardware fail-safe), divider gives ~2.39V ≈ raw 2966.
#define DIM_RAW_MIN              50      // noise-floor offset
#define DIM_RAW_MAX              3700    // wiper at +12V through divider
#define DIM_DEADBAND_RAW         30      // ±30 counts at endpoints

#define SAMPLE_PERIOD_MS         20      // 50 Hz
#define FAILSAFE_RAW_THRESHOLD   5       // "raw stuck at 0" with noise margin
#define FAILSAFE_HOLDOFF_MS      5000    // spec §4.7

// --- State ---------------------------------------------------------------

typedef struct {
    bool     initialized;
    uint16_t filtered;             // Q12 ADC value, IIR-smoothed
    uint32_t last_sample_ms;
    bool     zero_holdoff_active;
    uint32_t zero_started_ms;
    bool     failsafe_active;
} dimmer_state_t;

static dimmer_state_t g_d;

// --- Public API ---------------------------------------------------------

void dimmer_init(void) {
    memset(&g_d, 0, sizeof(g_d));
}

void dimmer_tick(uint32_t now_ms, uint16_t raw) {
    // Rate-limit to 50 Hz (first call always proceeds).
    if (g_d.initialized &&
        (uint32_t)(now_ms - g_d.last_sample_ms) < SAMPLE_PERIOD_MS) {
        return;
    }
    g_d.initialized    = true;
    g_d.last_sample_ms = now_ms;

    // IIR α = 1/4: filtered = (3*filtered + raw + 2) / 4
    // Time constant ≈ 4 samples ≈ 80ms at 50 Hz.
    g_d.filtered = (uint16_t)(((uint32_t)g_d.filtered * 3u + raw + 2u) / 4u);

    // Soft fail-safe: raw stuck near 0 for > 5s → flag failsafe (caller gets 50%).
    if (raw < FAILSAFE_RAW_THRESHOLD) {
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
    if (g_d.failsafe_active) {
        return 2048;  // 50%
    }

    const int32_t f = (int32_t)g_d.filtered;

    // Apply input-space deadbands at endpoints.
    if (f <= DIM_RAW_MIN + DIM_DEADBAND_RAW) return 0;
    if (f >= DIM_RAW_MAX - DIM_DEADBAND_RAW) return 4095;

    int32_t scaled = (f - DIM_RAW_MIN) * 4095 / (DIM_RAW_MAX - DIM_RAW_MIN);
    if (scaled < 0)    scaled = 0;
    if (scaled > 4095) scaled = 4095;
    return (uint16_t)scaled;
}

bool dimmer_failsafe_active(void) {
    return g_d.failsafe_active;
}
