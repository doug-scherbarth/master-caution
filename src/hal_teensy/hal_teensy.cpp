// src/hal_teensy/hal_teensy.cpp
// Teensy 4.1 HAL — spec v1.4 pin assignments.
//
// DB-15 #1 alarm inputs (active-low, internal pull-up):
//   CH[ 0] CH_CO_DETECT        → pin  0
//   CH[ 1] CH_L_MAG_FAIL       → pin  1
//   CH[ 2] CH_R_MAG_FAIL       → pin  5
//   CH[ 3] CH_OIL_PRESS_LOW    → pin  6
//   CH[ 4] CH_CHT_OVERTEMP     → pin  8
//   CH[ 5] CH_EGT_OVERTEMP     → pin  9
//   CH[ 6] CH_PRIM_ALT_FAIL    → pin 11
//   CH[ 7] CH_SEC_ALT_FAIL     → pin 12
//   CH[ 8] CH_PITOT_HEAT_FAIL  → pin 14
//   CH[ 9] CH_OIL_TEMP_HIGH    → pin 15
//   CH[10] CH_FUEL_PRESS_LOW   → pin 16
//   CH[11] CH_FLAPS_DEPLOYED   → pin 17
//   CH[12] CH_AIRSPEED_OVER_VFE→ pin 22
//   CH[13] CH_BOOST_PUMP_ON    → pin 23
//
// Button SW_MC (active-low, 4.7k + BAT54S) → pin 10
//
// Dimmer (ratiometric, §6.2):
//   DIM_IN    → pin 26 (A12)
//   BUS_SENSE → pin 27 (A13)
//
// Button LED (cathode-sink, §9.4):
//   Red   → pin 2   Blue  → pin 4
//   Green → pin 3   AUX   → not wired (HAL_LED_AUX writes are ignored)
//
// I2S (managed by Audio library):
//   pin  7 = I2S TX data (DIN → PCM5102)
//   pin 20 = I2S LRCLK
//   pin 21 = I2S BCLK
//
// WS2812 data → pin 29 (Serial7 TX) — not yet driven; pin left as input.
// Lighting buck EN → pin 30, PG → pin 31 — not yet configured.

#include <Arduino.h>
#include "hal.h"
#include "audio_glue.h"

static const uint8_t ALARM_PINS[14] = {
     0,  1,  5,  6,  8,  9, 11,   // CH[ 0.. 6]
    12, 14, 15, 16, 17, 22, 23     // CH[ 7..13]
};

#define PIN_BUTTON    10
#define PIN_DIM_IN    26   // A12
#define PIN_BUS_SENSE 27   // A13

static const uint8_t LED_PINS[3] = { 2, 3, 4 };  // R, G, B

void hal_init(void) {
    for (int i = 0; i < 14; i++) {
        pinMode(ALARM_PINS[i], INPUT_PULLUP);
    }
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    analogReadResolution(12);
    analogWriteResolution(12);

    for (int i = 0; i < 3; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        analogWrite(LED_PINS[i], 4095);  // cathode-sink: high = off
    }

    Serial.begin(115200);
    audio_glue_init();
}

uint32_t hal_millis(void) {
    return millis();
}

bool hal_read_alarm(uint8_t channel) {
    if (channel >= 14) return false;
    return !digitalRead(ALARM_PINS[channel]);  // active-low → positive logic
}

bool hal_read_button(void) {
    return !digitalRead(PIN_BUTTON);  // active-low → positive logic
}

uint16_t hal_adc_read(hal_adc_ch_t ch) {
    switch (ch) {
    case HAL_ADC_DIM_IN:    return (uint16_t)analogRead(PIN_DIM_IN);
    case HAL_ADC_BUS_SENSE: return (uint16_t)analogRead(PIN_BUS_SENSE);
    default:                return 0u;
    }
}

void hal_set_led_duty(hal_led_t led, uint16_t duty) {
    if (led >= 3u) return;  // HAL_LED_AUX not wired; future pixel_lighting
    // Cathode-sink: invert so duty 0 = off, 4095 = full bright.
    analogWrite(LED_PINS[led], 4095u - duty);
}

void hal_audio_play(uint8_t wav_id) {
    audio_glue_play(wav_id);
}

bool hal_audio_busy(void)  { return audio_glue_busy();  }
bool hal_audio_sd_ok(void) { return audio_glue_sd_ok(); }

void hal_log_write(const uint8_t *buf, size_t n) {
    Serial.write(buf, n);
}
