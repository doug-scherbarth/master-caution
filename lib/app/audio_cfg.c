// lib/app/audio_cfg.c
// INI-style AUDIO.CFG parser.

#include "audio_cfg.h"
#include "ini_util.h"
#include "ring_log.h"
#include "hal.h"
#include <string.h>
#include <stdlib.h>

#define AUDIO_CFG_FILE_MAX 2048u

static void set_defaults(audio_cfg_t *out) {
    out->gain = AUDIO_CFG_GAIN_DEFAULT;
}

void audio_cfg_parse(const char *text, audio_cfg_t *out) {
    set_defaults(out);
    if (!text || !text[0]) return;

    char line[128];
    while (ini_next_line(&text, line, sizeof(line)) || *text) {
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') continue;  // section headers ignored (only one section)

        char *eq = strchr(line, '=');
        if (!eq) continue;

        char key[32] = {0};
        int  klen    = (int)(eq - line);
        while (klen > 0 && (line[klen-1] == ' ' || line[klen-1] == '\t')) klen--;
        if (klen > 31) klen = 31;
        memcpy(key, line, (size_t)klen);
        ini_str_lower(key);

        char val[32] = {0};
        const char *vp = eq + 1;
        while (*vp == ' ' || *vp == '\t') vp++;
        int i = 0;
        while (vp[i] && i < 31) { val[i] = vp[i]; i++; }
        ini_strip_comment(val);

        if (strcmp(key, "gain") == 0) {
            char *end;
            float v = strtof(val, &end);
            if (end == val) {
                // non-numeric — keep default
            } else if (v < AUDIO_CFG_GAIN_MIN) {
                out->gain = AUDIO_CFG_GAIN_MIN;
            } else if (v > AUDIO_CFG_GAIN_MAX) {
                out->gain = AUDIO_CFG_GAIN_MAX;
            } else {
                out->gain = v;
            }
        }
    }
}

bool audio_cfg_load(audio_cfg_t *out) {
    static char s_file_buf[AUDIO_CFG_FILE_MAX];
    size_t file_len = 0;

    hal_sd_status_t sd_status =
        hal_sd_read_file("/AUDIO.CFG", s_file_buf, sizeof(s_file_buf), &file_len);
    if (sd_status != HAL_SD_OK) {
        if (sd_status == HAL_SD_TOO_BIG)
            ring_log_fault(LOG_FAULT_AUDIO_CFG, 0u);
        set_defaults(out);
        return false;
    }

    audio_cfg_parse(s_file_buf, out);
    return true;
}
