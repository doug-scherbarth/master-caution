// src/hal_teensy/audio_glue.h
// Internal C interface between hal_teensy.cpp and the Teensy Audio objects.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_glue_init(void);
void audio_glue_play(uint8_t wav_id);
bool audio_glue_busy(void);

#ifdef __cplusplus
}
#endif
