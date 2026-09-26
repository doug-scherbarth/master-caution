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
