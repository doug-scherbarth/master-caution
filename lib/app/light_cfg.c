// lib/app/light_cfg.c
// INI-style LIGHTS.CFG parser for WS2812B cabin/panel lighting.

#include "light_cfg.h"
#include "hal.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void build_gamma_lut(uint8_t *lut, float gamma) {
    lut[0] = 0;
    for (int i = 1; i < 255; i++) {
        float v = powf((float)i / 255.0f, gamma) * 255.0f + 0.5f;
        if (v > 255.0f) v = 255.0f;
        lut[i] = (uint8_t)v;
    }
    lut[255] = 255;
}

// Lowercase a key in-place (ASCII only).
static void str_lower(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s += 32;
}

// Extract one line from *pp, advance *pp, return length (0 = exhausted).
static int next_line(const char **pp, char *buf, int bufsz) {
    if (!**pp) return 0;
    int n = 0;
    while (**pp && **pp != '\n' && n < bufsz - 1)
        buf[n++] = *(*pp)++;
    if (**pp == '\n') (*pp)++;
    // strip trailing CR and spaces
    while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        n--;
    buf[n] = '\0';
    return n;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool light_cfg_parse(const char *text, light_cfg_t *out) {
    if (!text || !out) return false;
    memset(out, 0, sizeof(*out));

    // defaults
    out->total_pixels        = LIGHT_MAX_PIXELS;
    out->mA_per_channel      = 20;
    out->quiescent_mA        = 1;
    out->max_current_mA      = 2000;
    out->gesture_low_pct     = 20;
    out->gesture_high_pct    = 80;
    out->gesture_timeout_ms  = 2000;
    float gamma = 2.2f;

    typedef enum { SEC_NONE, SEC_GLOBAL, SEC_CONFIG } section_t;
    section_t         sec      = SEC_NONE;
    light_config_t   *cur_cfg  = NULL;
    bool              seg_open = false;
    light_seg_t       seg;
    memset(&seg, 0, sizeof(seg));

    char line[128];
    while (next_line(&text, line, sizeof(line)) || *text) {
        // skip blank and comment lines
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;

        // section header
        if (line[0] == '[') {
            // close any open segment
            if (sec == SEC_CONFIG && seg_open && cur_cfg) {
                if (cur_cfg->n_segs < LIGHT_MAX_SEGS)
                    cur_cfg->segs[cur_cfg->n_segs++] = seg;
                seg_open = false;
            }

            // strip trailing ']'
            char hdr[32];
            int hn = 0;
            for (int i = 1; line[i] && line[i] != ']' && hn < 31; i++)
                hdr[hn++] = line[i];
            hdr[hn] = '\0';
            str_lower(hdr);

            if (strcmp(hdr, "global") == 0) {
                sec = SEC_GLOBAL;
            } else if (strcmp(hdr, "config") == 0) {
                sec = SEC_CONFIG;
                if (out->n_configs < LIGHT_MAX_CONFIGS) {
                    cur_cfg = &out->configs[out->n_configs++];
                    memset(cur_cfg, 0, sizeof(*cur_cfg));
                    snprintf(cur_cfg->name, LIGHT_NAME_LEN, "cfg%u",
                             (unsigned)(out->n_configs - 1));
                } else {
                    cur_cfg = NULL;  // over limit, discard
                }
            } else {
                sec = SEC_NONE;
            }
            continue;
        }

        // key = value
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

        if (sec == SEC_GLOBAL) {
            if      (strcmp(key, "total_pixels")        == 0) out->total_pixels       = (uint16_t)atoi(vp);
            else if (strcmp(key, "ma_per_channel")      == 0) out->mA_per_channel     = (uint16_t)atoi(vp);
            else if (strcmp(key, "quiescent_ma")        == 0) out->quiescent_mA       = (uint16_t)atoi(vp);
            else if (strcmp(key, "max_current_ma")      == 0) out->max_current_mA     = (uint16_t)atoi(vp);
            else if (strcmp(key, "gamma")               == 0) gamma = strtof(vp, NULL);
            else if (strcmp(key, "gesture_low_pct")     == 0) { int v = atoi(vp); out->gesture_low_pct    = (uint8_t)(v < 1 ? 1 : v > 99 ? 99 : v); }
            else if (strcmp(key, "gesture_high_pct")    == 0) { int v = atoi(vp); out->gesture_high_pct   = (uint8_t)(v < 1 ? 1 : v > 99 ? 99 : v); }
            else if (strcmp(key, "gesture_timeout_ms")  == 0) out->gesture_timeout_ms = (uint16_t)atoi(vp);
        } else if (sec == SEC_CONFIG && cur_cfg) {
            if (strcmp(key, "start") == 0) {
                // start= always begins a new segment
                if (seg_open && cur_cfg->n_segs < LIGHT_MAX_SEGS)
                    cur_cfg->segs[cur_cfg->n_segs++] = seg;
                memset(&seg, 0, sizeof(seg));
                seg.scale_pct = 100;
                seg.r = seg.g = seg.b = 255;
                seg.start = (uint16_t)atoi(vp);
                seg_open = true;
            } else if (strcmp(key, "name") == 0) {
                strncpy(cur_cfg->name, vp, LIGHT_NAME_LEN - 1);
                cur_cfg->name[LIGHT_NAME_LEN - 1] = '\0';
            } else if (seg_open) {
                if      (strcmp(key, "count") == 0) seg.count     = (uint16_t)atoi(vp);
                else if (strcmp(key, "r")     == 0) seg.r         = (uint8_t)atoi(vp);
                else if (strcmp(key, "g")     == 0) seg.g         = (uint8_t)atoi(vp);
                else if (strcmp(key, "b")     == 0) seg.b         = (uint8_t)atoi(vp);
                else if (strcmp(key, "scale") == 0) seg.scale_pct = (uint8_t)atoi(vp);
            }
        }
    }

    // close final segment
    if (sec == SEC_CONFIG && seg_open && cur_cfg) {
        if (cur_cfg->n_segs < LIGHT_MAX_SEGS)
            cur_cfg->segs[cur_cfg->n_segs++] = seg;
    }

    if (out->total_pixels > LIGHT_MAX_PIXELS)
        out->total_pixels = LIGHT_MAX_PIXELS;

    build_gamma_lut(out->gamma_lut, gamma);

    return out->n_configs > 0;
}

void light_cfg_fallback(light_cfg_t *out) {
    memset(out, 0, sizeof(*out));
    out->total_pixels        = LIGHT_MAX_PIXELS;
    out->mA_per_channel      = 20;
    out->quiescent_mA        = 1;
    out->max_current_mA      = 2000;
    out->gesture_low_pct     = 20;
    out->gesture_high_pct    = 80;
    out->gesture_timeout_ms  = 2000;
    build_gamma_lut(out->gamma_lut, 2.2f);

    out->n_configs = 1;
    strncpy(out->configs[0].name, "default", LIGHT_NAME_LEN - 1);
    out->configs[0].n_segs      = 1;
    out->configs[0].segs[0].start     = 0;
    out->configs[0].segs[0].count     = LIGHT_MAX_PIXELS;
    out->configs[0].segs[0].r         = 255;
    out->configs[0].segs[0].g         = 255;
    out->configs[0].segs[0].b         = 255;
    out->configs[0].segs[0].scale_pct = 20;  // dim white
}

bool light_cfg_load(light_cfg_t *out) {
    static char s_file_buf[LIGHT_CFG_FILE_MAX];
    size_t file_len = 0;

    if (!hal_sd_read_file("/LIGHTS.CFG", s_file_buf, sizeof(s_file_buf), &file_len)) {
        light_cfg_fallback(out);
        return false;
    }

    if (!light_cfg_parse(s_file_buf, out)) {
        light_cfg_fallback(out);
        return false;
    }

    return true;
}
