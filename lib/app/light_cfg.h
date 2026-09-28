// lib/app/light_cfg.h
// LIGHTS.CFG parser and built-in fallback.
//
// Parse is done from a NUL-terminated string so it is fully host-testable
// without touching the filesystem.  light_cfg_load() wraps hal_sd_read_file.
//
// Gamma is stored as a precomputed 256-entry LUT (built at parse time) so
// the render path is integer-only at runtime.

#ifndef LIGHT_CFG_H
#define LIGHT_CFG_H

#include <stdint.h>
#include <stdbool.h>

#define LIGHT_MAX_PIXELS    150u
#define LIGHT_MAX_CONFIGS     8u
#define LIGHT_MAX_SEGS        4u
#define LIGHT_NAME_LEN       16u
#define LIGHT_CFG_FILE_MAX 4096u   // max /LIGHTS.CFG file size in bytes

typedef struct {
    uint16_t start;       // first pixel index in chain (0-based)
    uint16_t count;       // number of pixels in segment
    uint8_t  r, g, b;    // base colour (0-255 each)
    uint8_t  scale_pct;  // relative brightness 0-100%
} light_seg_t;

typedef struct {
    char        name[LIGHT_NAME_LEN];
    uint8_t     n_segs;
    light_seg_t segs[LIGHT_MAX_SEGS];
} light_config_t;

typedef struct {
    uint16_t       total_pixels;
    uint16_t       mA_per_channel;
    uint16_t       quiescent_mA;
    uint16_t       max_current_mA;
    // gamma_lut[i] = round(pow(i/255, gamma) * 255); built at parse time.
    uint8_t        gamma_lut[256];
    // Dimmer gesture thresholds — tunable without recompiling.
    uint8_t        gesture_low_pct;       // dip arms below this % of full range (0-100)
    uint8_t        gesture_high_pct;      // bump arms above this % of full range (0-100)
    uint16_t       gesture_timeout_ms;    // ms to complete gesture before it cancels
    // MC button LED floors — perceived percent, gamma-mapped to Q12 duty at runtime.
    uint8_t        mc_floor_ack_pct;      // perceived % floor for acked/steady, 1-100 (default 15)
    uint8_t        mc_floor_pending_pct;  // perceived % floor for pending/flashing, 1-100 (default 35)
    uint16_t       mc_floor_ack_q12;      // gamma-mapped Q12 floor, acked alarms
    uint16_t       mc_floor_pending_q12;  // gamma-mapped Q12 floor, pending alarms
    uint8_t        n_configs;
    light_config_t configs[LIGHT_MAX_CONFIGS];
} light_cfg_t;

// Parse a NUL-terminated INI-style string into *out.
// Returns true on success; false if the text is empty or has no valid [config].
// Extra configs / segments beyond the limits are silently dropped.
bool light_cfg_parse(const char *text, light_cfg_t *out);

// Populate *out with the built-in single-segment dim-white fallback.
void light_cfg_fallback(light_cfg_t *out);

// Load /LIGHTS.CFG from SD via hal_sd_read_file, parse it.
// On failure (missing file, parse error) calls light_cfg_fallback and returns false.
bool light_cfg_load(light_cfg_t *out);

#endif // LIGHT_CFG_H
