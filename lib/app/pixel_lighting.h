// lib/app/pixel_lighting.h
// WS2812B pixel lighting renderer.
//
// pixel_lighting_tick() is called every application frame.  It reads the
// dimmer ratio via HAL ADC, applies gamma, renders all segments of the active
// lighting config, enforces the per-config current limit, and calls
// hal_pixels_write().
//
// Call pixel_lighting_init() once after light_cfg_load() succeeds.

#ifndef PIXEL_LIGHTING_H
#define PIXEL_LIGHTING_H

#include "light_cfg.h"
#include <stdint.h>
#include <stdbool.h>

// Initialise the renderer with a parsed config and a starting config index.
// cfg must remain valid for the lifetime of the module.
void pixel_lighting_init(const light_cfg_t *cfg, uint8_t config_index);

// Render and output one frame.  Call once per application loop iteration.
void pixel_lighting_tick(void);

// Switch to a different config index (0-based).  Clamped to n_configs-1.
// Returns the index actually applied.
uint8_t pixel_lighting_set_config(uint8_t index);

// Return the currently active config index.
uint8_t pixel_lighting_get_config(void);

// Compute worst-case mA draw for a config at full brightness (no dimming).
// Used by current-limit logic and exposed for testing.
uint32_t pixel_lighting_max_ma(const light_cfg_t *cfg, uint8_t config_idx);

#endif // PIXEL_LIGHTING_H
