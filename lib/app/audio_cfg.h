// lib/app/audio_cfg.h
// INI-style AUDIO.CFG parser for audio amplifier settings.

#ifndef AUDIO_CFG_H
#define AUDIO_CFG_H

#include <stdbool.h>

#define AUDIO_CFG_GAIN_DEFAULT  0.30f
#define AUDIO_CFG_GAIN_MIN      0.00f   // 0 = mute
#define AUDIO_CFG_GAIN_MAX      2.00f

typedef struct {
    float gain;   // amplifier gain applied to both channels
} audio_cfg_t;

// Parse text from a loaded AUDIO.CFG.  Always fills *out (uses defaults on
// any parse error).  Caller owns the buffer lifetime.
void audio_cfg_parse(const char *text, audio_cfg_t *out);

// Load /AUDIO.CFG from the SD card and call audio_cfg_parse().
// Returns true if the file was found and parsed; false means defaults apply.
// Logs LOG_FAULT_AUDIO_CFG if the file is present but too large to read.
bool audio_cfg_load(audio_cfg_t *out);

#endif // AUDIO_CFG_H
