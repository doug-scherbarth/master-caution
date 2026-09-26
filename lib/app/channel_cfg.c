// lib/app/channel_cfg.c
// Per-channel alarm input configuration parser for /CHANNEL.CFG.
//
// File format (INI-style):
//   [defaults]          — applies values to all channels
//   [CHANNEL_NAME]      — overrides for that channel (e.g. [OIL_PRESS_LOW])
//   Keys: enabled (yes/no/true/false/1/0), debounce_ms, startup_excluded

#include "channel_cfg.h"
#include "channel_table.h"
#include "hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CHANNEL_CFG_FILE_MAX 2048u

// ---------------------------------------------------------------------------
// Internal helpers (identical pattern to light_cfg.c)
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
    return (strcmp(s, "yes") == 0 || strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void channel_cfg_fallback(channel_cfg_t *out) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        out->ch[i].debounce_ms      = CHANNEL_TABLE[i].debounce_ms;
        out->ch[i].enabled          = true;
        out->ch[i].startup_excluded = (i == CH_OIL_PRESS_LOW);
    }
}

void channel_cfg_parse(const char *text, channel_cfg_t *out) {
    // Start from compiled defaults; INI entries overlay from there.
    channel_cfg_fallback(out);
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
                    // Compare lowercased hdr against lowercased CHANNEL_TABLE name
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

        const char *vp = eq + 1;
        while (*vp == ' ' || *vp == '\t') vp++;

        if (sec == SEC_DEFAULTS) {
            if (strcmp(key, "debounce_ms") == 0) {
                uint16_t v = (uint16_t)atoi(vp);
                for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].debounce_ms = v;
            } else if (strcmp(key, "enabled") == 0) {
                bool v = parse_bool(vp);
                for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].enabled = v;
            } else if (strcmp(key, "startup_excluded") == 0) {
                bool v = parse_bool(vp);
                for (uint8_t i = 0; i < CHANNEL_COUNT; i++) out->ch[i].startup_excluded = v;
            }
        } else if (sec == SEC_CHANNEL && cur_ch >= 0) {
            if      (strcmp(key, "debounce_ms")      == 0) out->ch[cur_ch].debounce_ms      = (uint16_t)atoi(vp);
            else if (strcmp(key, "enabled")          == 0) out->ch[cur_ch].enabled          = parse_bool(vp);
            else if (strcmp(key, "startup_excluded") == 0) out->ch[cur_ch].startup_excluded = parse_bool(vp);
        }
    }
}

bool channel_cfg_load(channel_cfg_t *out) {
    static char s_buf[CHANNEL_CFG_FILE_MAX];
    size_t len = 0;
    if (!hal_sd_read_file("/CHANNEL.CFG", s_buf, sizeof(s_buf), &len)) {
        channel_cfg_fallback(out);
        return false;
    }
    channel_cfg_parse(s_buf, out);
    return true;
}
