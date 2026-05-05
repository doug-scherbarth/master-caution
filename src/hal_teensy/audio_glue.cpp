// src/hal_teensy/audio_glue.cpp
// Teensy Audio Library wrapper — plays WAV files from the built-in SD card.
//
// Files must be 16-bit PCM 44100 Hz stereo, named 0.WAV, 1.WAV, …
// on a FAT32 microSD inserted in the Teensy 4.1's underside slot.
//
// Wiring (PCM5102A breakout):
//   Teensy pin 7  → PCM5102A DIN
//   Teensy pin 20 → PCM5102A LRCK
//   Teensy pin 21 → PCM5102A BCK
//   FMT/SCK/FLT/DEMP/XSMT pulled per PCM5102A datasheet defaults

#include <Audio.h>
#include <SD.h>
#include "audio_glue.h"

static bool             g_sd_ok = false;
static AudioPlaySdWav   g_player;
static AudioAmplifier   g_amp_l;
static AudioAmplifier   g_amp_r;
static AudioOutputI2S   g_i2s;
static AudioConnection  g_patch_pl(g_player, 0, g_amp_l, 0);
static AudioConnection  g_patch_pr(g_player, 1, g_amp_r, 0);
static AudioConnection  g_patch_al(g_amp_l,  0, g_i2s,   0);
static AudioConnection  g_patch_ar(g_amp_r,  0, g_i2s,   1);

#define OUTPUT_GAIN 0.3f

void audio_glue_init(void) {
    AudioMemory(24);
    g_amp_l.gain(OUTPUT_GAIN);
    g_amp_r.gain(OUTPUT_GAIN);
    if (SD.begin(BUILTIN_SDCARD)) {
        File f = SD.open("0.WAV");
        g_sd_ok = (bool)f;
        if (f) f.close();
    }
}

void audio_glue_play(uint8_t wav_id) {
    char name[8];
    snprintf(name, sizeof(name), "%u.WAV", wav_id);
    g_player.play(name);
}

bool audio_glue_busy(void) { return g_player.isPlaying(); }
bool audio_glue_sd_ok(void) { return g_sd_ok; }
