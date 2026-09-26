// lib/app/channel_cfg.h
//
// Per-channel alarm input configuration loaded from /CHANNEL.CFG on the SD
// card at boot. Allows in-field tuning of enabled state, debounce timing, and
// startup-exclusion without recompiling. Falls back to safe defaults if the
// file is absent or unreadable.

#ifndef CHANNEL_CFG_H
#define CHANNEL_CFG_H

#include "types.h"

typedef struct {
    uint16_t debounce_ms;       // qualification window for this channel
    bool     startup_excluded;  // skip this channel in the startup fault check
    bool     enabled;           // false = treat channel as always-inactive
} channel_entry_t;

typedef struct {
    channel_entry_t ch[CHANNEL_COUNT];
} channel_cfg_t;

// Parse text in INI format into *out. Always succeeds: unknown sections and
// keys are silently ignored, parsed values overlay the fallback defaults.
void channel_cfg_parse(const char *text, channel_cfg_t *out);

// Fill *out with built-in defaults: all channels enabled, debounce_ms from
// CHANNEL_TABLE, OIL_PRESS_LOW startup-excluded, all others included.
void channel_cfg_fallback(channel_cfg_t *out);

// Read /CHANNEL.CFG from SD, parse it, and store in *out. Returns true if the
// file was found and parsed; false if the fallback was used instead.
bool channel_cfg_load(channel_cfg_t *out);

#endif // CHANNEL_CFG_H
