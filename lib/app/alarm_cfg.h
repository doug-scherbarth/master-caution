// lib/app/alarm_cfg.h
// Per-channel alarm input configuration read from /ALARMS.CFG at boot.
//
// Compiled-in defaults (alarm_cfg_fallback) are always applied first.
// SD settings overlay only polarity, pull, and debounce — channel enables,
// severity, and composite logic are never SD-configurable.
//
// A missing SD file is not an error (silently uses defaults).
// A file that exists but cannot be read causes ALARM_CFG_ERROR.
// A file that parses but contains suspicious values causes ALARM_CFG_WARN.
// Both ERROR and WARN surface through the startup CH_FAULT warning path.

#ifndef ALARM_CFG_H
#define ALARM_CFG_H

#include <stdint.h>
#include <stdbool.h>
#include "channel_table.h"

// Mirror of hal_pull_t — values are intentionally identical so app.c may
// cast directly; the enum lives here to keep alarm_cfg.h free of hal.h.
typedef enum {
    ALARM_PULL_DOWN = 0,
    ALARM_PULL_UP   = 1,
    ALARM_PULL_NONE = 2,
} alarm_pull_t;

typedef struct {
    uint16_t    debounce_ms;  // 50–5000 ms; out-of-range → compiled default
    bool        active_high;  // true = assert HIGH (default)
    alarm_pull_t pull;        // default: ALARM_PULL_DOWN
} alarm_entry_t;

typedef struct {
    alarm_entry_t ch[CHANNEL_COUNT];
} alarm_cfg_t;

typedef enum {
    ALARM_CFG_NO_FILE = 0,  // /ALARMS.CFG absent — silently using defaults
    ALARM_CFG_OK      = 1,  // file found and parsed cleanly
    ALARM_CFG_ERROR   = 2,  // file found but unreadable — alarm polarity may be wrong
    ALARM_CFG_WARN    = 3,  // file parsed but one or more suspicious values found
} alarm_cfg_status_t;

// Seed all channels with safe compiled defaults.
void               alarm_cfg_fallback(alarm_cfg_t *out);

// Parse INI text into *out.  Calls fallback first; [defaults] applied before
// named sections regardless of order.  Returns warning count (0 = clean).
// Warnings: unknown section, unknown key, bad value, key outside section,
// polarity mismatch (active-high+pull-up or active-low+pull-down).
uint8_t            alarm_cfg_parse(const char *text, alarm_cfg_t *out);

// Read /ALARMS.CFG from SD.  Returns ALARM_CFG_NO_FILE if absent (fallback used),
// ALARM_CFG_OK if parsed, ALARM_CFG_ERROR if file found but unreadable.
alarm_cfg_status_t alarm_cfg_load(alarm_cfg_t *out);

// Print effective configuration table to hal_log_write (USB serial at runtime).
void               alarm_cfg_dump(const alarm_cfg_t *cfg, alarm_cfg_status_t status);

#endif // ALARM_CFG_H
