// src/pintest/audiotest.cpp
// PCM5102A I2S bring-up — compiled with env:audiotest only.
// No SD required — uses AudioSynthWaveformSine directly.
//
// Cycles: 440 Hz (1 s) → 880 Hz (1 s) → 1320 Hz (1 s) → silence (0.5 s) → repeat.
// USB serial reports the active frequency so you can correlate what you hear.
//
// I2S wiring (must be present):
//   Teensy pin 7  → PCM5102A DIN
//   Teensy pin 20 → PCM5102A LRCK
//   Teensy pin 21 → PCM5102A BCK
//
// PCM5102A control pins: FMT=GND (I2S), SCK=GND (no MCLK), FLT/DEMP/XSMT per datasheet.

#include <Arduino.h>
#include <Audio.h>

static AudioSynthWaveformSine  sine;
static AudioAmplifier          amp_l;
static AudioAmplifier          amp_r;
static AudioOutputI2S          i2s_out;
static AudioConnection         patch_sl(sine,  0, amp_l, 0);
static AudioConnection         patch_sr(sine,  0, amp_r, 0);
static AudioConnection         patch_al(amp_l, 0, i2s_out, 0);
static AudioConnection         patch_ar(amp_r, 0, i2s_out, 1);

#define OUTPUT_GAIN  0.3f

static const struct { float freq; const char *label; uint32_t dur_ms; } STEPS[] = {
    { 440.0f,  "440 Hz  (A4)",  1000 },
    { 880.0f,  "880 Hz  (A5)",  1000 },
    { 1320.0f, "1320 Hz (E6)", 1000 },
    { 0.0f,    "silence",       500  },
};
static const uint8_t NSTEPS = sizeof(STEPS) / sizeof(STEPS[0]);

void setup(void) {
    Serial.begin(115200);
    AudioMemory(16);
    amp_l.gain(OUTPUT_GAIN);
    amp_r.gain(OUTPUT_GAIN);
    sine.amplitude(0);
    sine.frequency(440.0f);
    Serial.println("\n=== PCM5102A I2S audio test ===");
}

void loop(void) {
    static uint8_t  step      = 0;
    static uint32_t step_start = 0;

    if ((uint32_t)(millis() - step_start) >= STEPS[step].dur_ms) {
        step = (step + 1) % NSTEPS;
        step_start = millis();

        if (STEPS[step].freq > 0.0f) {
            sine.frequency(STEPS[step].freq);
            sine.amplitude(1.0f);
        } else {
            sine.amplitude(0.0f);
        }
        Serial.printf("→ %s\n", STEPS[step].label);
    }
}
