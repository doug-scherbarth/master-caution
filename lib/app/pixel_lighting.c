// lib/app/pixel_lighting.c
// WS2812B pixel renderer: brightness → gamma → per-segment colour → current
// limit → hal_pixels_write().  Rate-limited to 50 Hz.

#include "pixel_lighting.h"
#include "hal.h"
#include <string.h>

#define RENDER_PERIOD_MS 20u   // 50 Hz

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static const light_cfg_t *s_cfg         = NULL;
static uint8_t            s_config_idx  = 0;
static bool               s_rendered    = false;
static uint32_t           s_last_render_ms;

// Scratch render buffer: 3 bytes per pixel (R, G, B).
static uint8_t s_buf[LIGHT_MAX_PIXELS * 3];

// ---------------------------------------------------------------------------
// Current-limit helper
// ---------------------------------------------------------------------------

// Sum peak mA across all pixels in a config at full brightness (dimmer=255).
// Each active channel draws mA_per_channel; each pixel draws quiescent_mA.
// A pixel is counted as active if any segment covers it.
uint32_t pixel_lighting_max_ma(const light_cfg_t *cfg, uint8_t config_idx) {
    if (!cfg || config_idx >= cfg->n_configs) return 0;
    const light_config_t *c = &cfg->configs[config_idx];
    uint32_t total = 0;
    for (uint8_t s = 0; s < c->n_segs; s++) {
        const light_seg_t *seg = &c->segs[s];
        uint16_t end = seg->start + seg->count;
        if (end > cfg->total_pixels) end = cfg->total_pixels;
        uint16_t n = (end > seg->start) ? (end - seg->start) : 0;
        // worst-case: all channels on at full scale_pct
        uint32_t r = (uint32_t)seg->r * seg->scale_pct / 100u;
        uint32_t g = (uint32_t)seg->g * seg->scale_pct / 100u;
        uint32_t b = (uint32_t)seg->b * seg->scale_pct / 100u;
        uint32_t ch_ma = ((r + g + b) * cfg->mA_per_channel + 127u) / 255u;
        total += (ch_ma + cfg->quiescent_mA) * n;
    }
    return total;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void pixel_lighting_init(const light_cfg_t *cfg, uint8_t config_index) {
    s_cfg = cfg;
    s_config_idx = 0;
    if (cfg && config_index < cfg->n_configs)
        s_config_idx = config_index;
    s_rendered = false;
    s_last_render_ms = 0;
    memset(s_buf, 0, sizeof(s_buf));
}

uint8_t pixel_lighting_set_config(uint8_t index) {
    if (!s_cfg) return 0;
    if (index >= s_cfg->n_configs) index = (uint8_t)(s_cfg->n_configs - 1u);
    s_config_idx = index;
    return s_config_idx;
}

uint8_t pixel_lighting_get_config(void) { return s_config_idx; }

void pixel_lighting_tick(uint32_t now_ms, uint8_t brightness) {
    if (!s_cfg || s_cfg->n_configs == 0) return;

    // Rate-limit rendering to 50 Hz
    if (s_rendered && (uint32_t)(now_ms - s_last_render_ms) < RENDER_PERIOD_MS) return;
    s_last_render_ms = now_ms;
    s_rendered = true;

    // Apply gamma LUT to the brightness
    uint8_t gamma_ratio = s_cfg->gamma_lut[brightness];

    // --- Clear buffer
    uint16_t n = s_cfg->total_pixels;
    if (n > LIGHT_MAX_PIXELS) n = LIGHT_MAX_PIXELS;
    memset(s_buf, 0, (size_t)n * 3u);

    // --- Render segments
    const light_config_t *c = &s_cfg->configs[s_config_idx];
    for (uint8_t s = 0; s < c->n_segs; s++) {
        const light_seg_t *seg = &c->segs[s];
        uint16_t end = seg->start + seg->count;
        if (end > n) end = n;
        if (seg->start >= end) continue;

        // Final per-channel value = base × scale_pct/100 × gamma_ratio/255
        uint8_t r = (uint8_t)((uint32_t)seg->r * seg->scale_pct * gamma_ratio / 25500u);
        uint8_t g = (uint8_t)((uint32_t)seg->g * seg->scale_pct * gamma_ratio / 25500u);
        uint8_t b = (uint8_t)((uint32_t)seg->b * seg->scale_pct * gamma_ratio / 25500u);

        for (uint16_t i = seg->start; i < end; i++) {
            s_buf[i * 3u + 0u] = r;
            s_buf[i * 3u + 1u] = g;
            s_buf[i * 3u + 2u] = b;
        }
    }

    // --- Current limit: if rendered draw exceeds budget, scale down uniformly
    // Estimate mA from buffer: sum(R+G+B)/255 * mA_per_channel + n*quiescent
    uint32_t sum_ch = 0;
    for (uint16_t i = 0; i < n; i++)
        sum_ch += s_buf[i*3u] + s_buf[i*3u+1u] + s_buf[i*3u+2u];
    uint32_t est_ma = (sum_ch * s_cfg->mA_per_channel + 127u) / 255u
                    + (uint32_t)n * s_cfg->quiescent_mA;

    if (est_ma > s_cfg->max_current_mA && est_ma > 0u) {
        // Scale every channel down proportionally
        // new_val = val * max_current_mA / est_ma
        for (uint16_t i = 0; i < n * 3u; i++) {
            uint32_t v = (uint32_t)s_buf[i] * s_cfg->max_current_mA / est_ma;
            s_buf[i] = (v > 255u) ? 255u : (uint8_t)v;
        }
    }

    hal_pixels_write(s_buf, n);
}
