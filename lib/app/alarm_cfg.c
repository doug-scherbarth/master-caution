// lib/app/alarm_cfg.c
// Per-channel alarm input configuration parser for /ALARMS.CFG.
//
// File format (INI-style):
//   [defaults]        — applies values to every channel before named overrides;
//                       processed in a first pass so its position in the file
//                       does not matter.
//   [CHANNEL_NAME]    — overrides for one channel (e.g. [OIL_PRESS_LOW])
//
// Keys (case-insensitive value):
//   active_high  = yes | no | true | false | 1 | 0  (default: yes)
//   pull         = down | up | none                  (default: down)
//   debounce_ms  = 50-5000                           (default: per CHANNEL_TABLE)
//
// Inline ';' and '#' comments are stripped from values.
// Per-key fallback: an unrecognised or out-of-range value is kept at its
// compiled default and counted as a warning.

#include "alarm_cfg.h"
#include "ini_util.h"
#include "channel_table.h"
#include "alarm_table.h"
#include "hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ALARM_CFG_FILE_MAX  8192u
#define DEBOUNCE_MIN_MS       50u
#define DEBOUNCE_MAX_MS     5000u

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool try_parse_bool(const char *s, bool *out) {
    if (strcmp(s, "yes") == 0 || strcmp(s, "true") == 0 || strcmp(s, "1") == 0) {
        *out = true;  return true;
    }
    if (strcmp(s, "no") == 0 || strcmp(s, "false") == 0 || strcmp(s, "0") == 0) {
        *out = false; return true;
    }
    return false;
}

static bool try_parse_pull(const char *s, alarm_pull_t *out) {
    if (strcmp(s, "down") == 0) { *out = ALARM_PULL_DOWN; return true; }
    if (strcmp(s, "up")   == 0) { *out = ALARM_PULL_UP;   return true; }
    if (strcmp(s, "none") == 0) { *out = ALARM_PULL_NONE; return true; }
    return false;
}

// Look up a section header string in CHANNEL_TABLE; returns index or -1.
static int8_t find_channel(const char *hdr_lower) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        char tbl[32] = {0};
        int  tlen = 0;
        for (const char *p = CHANNEL_TABLE[i].name; *p && tlen < 31; p++)
            tbl[tlen++] = *p;
        ini_str_lower(tbl);
        if (strcmp(hdr_lower, tbl) == 0) return (int8_t)i;
    }
    return -1;
}

// Single parse pass over text.
// defaults_pass=true  → only applies [defaults] section; no warnings counted.
// defaults_pass=false → only applies named channel sections; counts warnings.
static uint8_t parse_pass(const char *text, alarm_cfg_t *out, bool defaults_pass) {
    typedef enum { SEC_NONE, SEC_DEFAULTS, SEC_CHANNEL } section_t;
    section_t sec    = SEC_NONE;
    int8_t    cur_ch = -1;
    uint8_t   warns  = 0;

    char line[128];
    while (ini_next_line(&text, line, sizeof(line)) || *text) {
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            char hdr[32] = {0};
            int  hn = 0;
            for (int i = 1; line[i] && line[i] != ']' && hn < 31; i++)
                hdr[hn++] = line[i];
            hdr[hn] = '\0';
            ini_str_lower(hdr);

            if (strcmp(hdr, "defaults") == 0) {
                sec = SEC_DEFAULTS; cur_ch = -1;
            } else {
                sec = SEC_CHANNEL;
                cur_ch = find_channel(hdr);
                if (!defaults_pass && cur_ch < 0) warns++;  // unknown section
            }
            continue;
        }

        // Key-value line: skip if not in the right pass or not in a section.
        if (defaults_pass  && sec != SEC_DEFAULTS) continue;
        if (!defaults_pass && sec == SEC_DEFAULTS)  continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        // Key outside any section (named-sections pass only).
        if (!defaults_pass && sec == SEC_NONE) { warns++; continue; }

        // Extract and lower-case the key.
        char key[32] = {0};
        int  klen    = (int)(eq - line);
        while (klen > 0 && (line[klen-1] == ' ' || line[klen-1] == '\t')) klen--;
        if (klen > 31) klen = 31;
        memcpy(key, line, (size_t)klen);
        ini_str_lower(key);

        // Extract value; strip inline comments and trailing whitespace.
        char        val[32] = {0};
        const char *vp      = eq + 1;
        while (*vp == ' ' || *vp == '\t') vp++;
        int i = 0;
        while (vp[i] && i < 31) { val[i] = vp[i]; i++; }
        ini_strip_comment(val);
        ini_str_lower(val);

        bool known_key = false;

        if (strcmp(key, "active_high") == 0) {
            known_key = true;
            bool v;
            if (try_parse_bool(val, &v)) {
                if (sec == SEC_DEFAULTS)
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].active_high = v;
                else if (cur_ch >= 0)
                    out->ch[cur_ch].active_high = v;
            } else {
                if (!defaults_pass) warns++;
            }
        } else if (strcmp(key, "pull") == 0) {
            known_key = true;
            alarm_pull_t p;
            if (try_parse_pull(val, &p)) {
                if (sec == SEC_DEFAULTS)
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].pull = p;
                else if (cur_ch >= 0)
                    out->ch[cur_ch].pull = p;
            } else {
                if (!defaults_pass) warns++;
            }
        } else if (strcmp(key, "debounce_ms") == 0) {
            known_key = true;
            char *end;
            long  v = strtol(val, &end, 10);
            if (end == val || *end != '\0') {
                if (!defaults_pass) warns++;       // non-numeric or empty
            } else if (v < DEBOUNCE_MIN_MS || v > DEBOUNCE_MAX_MS) {
                if (!defaults_pass) warns++;       // out of range (catches overflow)
            } else {
                uint16_t dv = (uint16_t)v;
                if (sec == SEC_DEFAULTS)
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].debounce_ms = dv;
                else if (cur_ch >= 0)
                    out->ch[cur_ch].debounce_ms = dv;
            }
        } else if (strcmp(key, "wav_id") == 0) {
            // Only meaningful in a named channel section (not [defaults]).
            // Spec §2.3: wav_id 0..WAV_COUNT-1; out-of-range → warning, compiled default kept.
            known_key = (sec == SEC_CHANNEL);
            if (!defaults_pass && sec == SEC_CHANNEL && cur_ch >= 0) {
                char *end;
                long  v = strtol(val, &end, 10);
                if (end == val || *end != '\0' || v < 0 || v >= WAV_COUNT) {
                    warns++;  // non-numeric, empty, or out of range
                } else {
                    out->ch[cur_ch].wav_id_override = (uint8_t)v;
                }
            } else if (!defaults_pass && sec == SEC_DEFAULTS) {
                known_key = false;  // wav_id in [defaults] is unknown
            }
        }

        if (!known_key && !defaults_pass) warns++;  // unknown key
    }
    return warns;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void alarm_cfg_fallback(alarm_cfg_t *out) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        out->ch[i].debounce_ms    = CHANNEL_TABLE[i].debounce_ms;
        out->ch[i].active_high    = true;
        out->ch[i].pull           = ALARM_PULL_DOWN;
        out->ch[i].wav_id_override = 0xFF;  // no override; use ALARM_TABLE compiled default
    }
}

uint8_t alarm_cfg_parse(const char *text, alarm_cfg_t *out) {
    alarm_cfg_fallback(out);
    if (!text || !text[0]) return 0;

    // Two-pass: defaults first (order-independent), then named sections.
    parse_pass(text, out, true);
    uint8_t warns = parse_pass(text, out, false);

    // Polarity mismatch: open wire would drive the pin to the alarm state.
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        bool         ah = out->ch[i].active_high;
        alarm_pull_t p  = out->ch[i].pull;
        if ((ah && p == ALARM_PULL_UP) || (!ah && p == ALARM_PULL_DOWN))
            warns++;
    }

    return warns;
}

alarm_cfg_status_t alarm_cfg_load(alarm_cfg_t *out) {
    static char s_buf[ALARM_CFG_FILE_MAX];
    size_t len = 0;
    hal_sd_status_t sd_st = hal_sd_read_file("/ALARMS.CFG", s_buf, sizeof(s_buf), &len);
    if (sd_st == HAL_SD_TOO_BIG || sd_st == HAL_SD_IO_ERROR) {
        alarm_cfg_fallback(out);
        return ALARM_CFG_ERROR;
    }
    if (sd_st != HAL_SD_OK) {
        alarm_cfg_fallback(out);
        return ALARM_CFG_NO_FILE;
    }
    if (len == 0) {
        alarm_cfg_fallback(out);
        return ALARM_CFG_NO_FILE;
    }
    uint8_t warns = alarm_cfg_parse(s_buf, out);
    return (warns > 0) ? ALARM_CFG_WARN : ALARM_CFG_OK;
}

void alarm_cfg_dump(const alarm_cfg_t *cfg, alarm_cfg_status_t status) {
    const char *src = status == ALARM_CFG_OK     ? "/ALARMS.CFG"           :
                      status == ALARM_CFG_WARN   ? "/ALARMS.CFG (warnings)" :
                      status == ALARM_CFG_NO_FILE ? "compiled defaults"     :
                                                    "ERROR-compiled defaults";
    char line[72];
    int  n = snprintf(line, sizeof(line), "ALARMS CFG (%s):\r\n", src);
    hal_log_write((const uint8_t *)line, (size_t)n);

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        const char *pull_s = cfg->ch[i].pull == ALARM_PULL_UP   ? "up  " :
                             cfg->ch[i].pull == ALARM_PULL_DOWN ? "down" : "none";
        n = snprintf(line, sizeof(line), "  [%2u] %-18s %c %s %4ums\r\n",
                     i,
                     CHANNEL_TABLE[i].name,
                     cfg->ch[i].active_high ? 'H' : 'L',
                     pull_s,
                     cfg->ch[i].debounce_ms);
        hal_log_write((const uint8_t *)line, (size_t)n);
    }
}
