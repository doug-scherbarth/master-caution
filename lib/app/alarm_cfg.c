// lib/app/alarm_cfg.c
// Per-channel alarm input configuration parser for /ALARMS.CFG.
//
// File format (INI-style):
//   [defaults]        — applies values to every channel before named overrides
//   [CHANNEL_NAME]    — overrides for one channel (e.g. [OIL_PRESS_LOW])
//
// Keys (case-insensitive value for bool):
//   active_high  = yes | no | true | false | 1 | 0  (default: yes)
//   pull         = down | up | none                  (default: down)
//   debounce_ms  = 50-5000                           (default: per CHANNEL_TABLE)
//
// Per-key fallback: an unrecognised or out-of-range value is silently ignored
// and the compiled default for that key is kept.

#include "alarm_cfg.h"
#include "channel_table.h"
#include "hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ALARM_CFG_FILE_MAX  2048u
#define DEBOUNCE_MIN_MS       50u
#define DEBOUNCE_MAX_MS     5000u

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void str_lower(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s += 32;
}

static int next_line(const char **pp, char *buf, int bufsz) {
    if (!**pp) return 0;
    int n = 0;
    while (**pp && **pp != '\n' && n < bufsz - 1)
        buf[n++] = *(*pp)++;
    if (**pp == '\n') (*pp)++;
    while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        n--;
    buf[n] = '\0';
    return n;
}

static bool parse_bool(const char *s) {
    return strcmp(s, "yes") == 0 || strcmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

// Returns true and sets *out only if the value is a recognised bool keyword.
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

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void alarm_cfg_fallback(alarm_cfg_t *out) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        out->ch[i].debounce_ms = CHANNEL_TABLE[i].debounce_ms;
        out->ch[i].active_high = true;
        out->ch[i].pull        = ALARM_PULL_DOWN;
    }
}

void alarm_cfg_parse(const char *text, alarm_cfg_t *out) {
    alarm_cfg_fallback(out);
    if (!text) return;

    typedef enum { SEC_NONE, SEC_DEFAULTS, SEC_CHANNEL } section_t;
    section_t sec    = SEC_NONE;
    int8_t    cur_ch = -1;

    char line[128];
    while (next_line(&text, line, sizeof(line)) || *text) {
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            char hdr[32];
            int  hn = 0;
            for (int i = 1; line[i] && line[i] != ']' && hn < 31; i++)
                hdr[hn++] = line[i];
            hdr[hn] = '\0';
            str_lower(hdr);

            if (strcmp(hdr, "defaults") == 0) {
                sec    = SEC_DEFAULTS;
                cur_ch = -1;
            } else {
                sec    = SEC_CHANNEL;
                cur_ch = -1;
                for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
                    char tbl[32];
                    int  tlen = 0;
                    for (const char *p = CHANNEL_TABLE[i].name; *p && tlen < 31; p++)
                        tbl[tlen++] = *p;
                    tbl[tlen] = '\0';
                    str_lower(tbl);
                    if (strcmp(hdr, tbl) == 0) { cur_ch = (int8_t)i; break; }
                }
            }
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;

        char key[32] = {0};
        int  klen    = (int)(eq - line);
        while (klen > 0 && (line[klen-1] == ' ' || line[klen-1] == '\t')) klen--;
        if (klen >= 32) klen = 31;
        memcpy(key, line, (size_t)klen);
        str_lower(key);

        char val[32] = {0};
        const char *vp = eq + 1;
        while (*vp == ' ' || *vp == '\t') vp++;
        int vlen = 0;
        while (vp[vlen] && vlen < 31) vlen++;
        memcpy(val, vp, (size_t)vlen);
        str_lower(val);

        if (sec == SEC_DEFAULTS) {
            if (strcmp(key, "active_high") == 0) {
                bool v;
                if (try_parse_bool(val, &v))
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].active_high = v;
            } else if (strcmp(key, "pull") == 0) {
                alarm_pull_t p;
                if (try_parse_pull(val, &p))
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].pull = p;
            } else if (strcmp(key, "debounce_ms") == 0) {
                uint16_t v = (uint16_t)atoi(val);
                if (v >= DEBOUNCE_MIN_MS && v <= DEBOUNCE_MAX_MS)
                    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].debounce_ms = v;
            }
        } else if (sec == SEC_CHANNEL && cur_ch >= 0) {
            if (strcmp(key, "active_high") == 0) {
                bool v;
                if (try_parse_bool(val, &v)) out->ch[cur_ch].active_high = v;
            } else if (strcmp(key, "pull") == 0) {
                alarm_pull_t p;
                if (try_parse_pull(val, &p)) out->ch[cur_ch].pull = p;
            } else if (strcmp(key, "debounce_ms") == 0) {
                uint16_t v = (uint16_t)atoi(val);
                if (v >= DEBOUNCE_MIN_MS && v <= DEBOUNCE_MAX_MS)
                    out->ch[cur_ch].debounce_ms = v;
                // out-of-range: silently keep per-key fallback (already set by fallback())
            }
        }
    }
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
        // Empty file — use fallback silently, same as absent.
        alarm_cfg_fallback(out);
        return ALARM_CFG_NO_FILE;
    }
    alarm_cfg_parse(s_buf, out);
    return ALARM_CFG_OK;
}

void alarm_cfg_dump(const alarm_cfg_t *cfg, alarm_cfg_status_t status) {
    const char *src = status == ALARM_CFG_OK      ? "/ALARMS.CFG"          :
                      status == ALARM_CFG_NO_FILE  ? "compiled defaults"    :
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
