// src/hal_teensy/hal_teensy.cpp
// Teensy 4.1 HAL implementation — breadboard prototype pin assignments.
//
// Alarm inputs (active-low, internal pull-up; only CH[0]/pin 0 is wired):
//   CH[ 0] CH_CO_DETECT        → pin  0   ← wire your test alarm here
//   CH[ 1] CH_L_MAG_FAIL       → pin  1
//   CH[ 2] CH_R_MAG_FAIL       → pin  2
//   CH[ 3] CH_OIL_PRESS_LOW    → pin  3
//   CH[ 4] CH_CHT_OVERTEMP     → pin  4
//   CH[ 5] CH_EGT_OVERTEMP     → pin  5
//   CH[ 6] CH_PRIM_ALT_FAIL    → pin  6
//   CH[ 7] CH_SEC_ALT_FAIL     → pin  9   (7/8 reserved for I2S)
//   CH[ 8] CH_PITOT_HEAT_FAIL  → pin 10
//   CH[ 9] CH_OIL_TEMP_HIGH    → pin 11
//   CH[10] CH_FUEL_PRESS_LOW   → pin 12
//   CH[11] CH_FLAPS_DEPLOYED   → pin 14
//   CH[12] CH_AIRSPEED_OVER_VFE→ pin 15
//   CH[13] CH_BOOST_PUMP_ON    → pin 16
//   CH[14] CH_SPARE            → pin 17
//
// Button (active-low, internal pull-up) → pin 18
// Dimmer pot (0–3.3 V)                  → pin 22 (A8)
//
// LEDs — cathode-sink breadboard wiring (VCC → LED → resistor → Teensy pin).
// duty 0 = off, 4095 = full bright (HAL inverts to 4095-duty before analogWrite).
// Pins 26/27 are not PWM-capable on Teensy 4.1; use 24/28 instead.
//   Red   → pin 25
//   Green → pin 24
//   Blue  → pin 28
//   Aux   → pin 29
//
// I2S (managed by Audio library, do not use for GPIO):
//   pin  7 = I2S TX data
//   pin 20 = I2S LRCLK
//   pin 21 = I2S BCLK
//   pin 23 = I2S MCLK

#include <Arduino.h>
#include "hal.h"
#include "audio_glue.h"

static const uint8_t ALARM_PINS[15] = {
     0,  1,  2,  3,  4,  5,  6,   // CH[ 0.. 6]
     9, 10, 11, 12, 14, 15, 16, 17 // CH[ 7..14]; skip 7,8 (I2S), 13 (built-in LED)
};

#define PIN_BUTTON  18
#define PIN_DIMMER  22  // A8

// HAL_LED_RED=0, GREEN=1, BLUE=2, AUX=3 — order must match hal_led_t enum.
static const uint8_t LED_PINS[4] = { 25, 24, 28, 29 };

void hal_init(void) {
    for (int i = 0; i < 15; i++) {
        pinMode(ALARM_PINS[i], INPUT_PULLUP);
    }
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    analogReadResolution(12);
    analogWriteResolution(12);

    for (int i = 0; i < 4; i++) {
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
    if (channel >= 15) return false;
    return !digitalRead(ALARM_PINS[channel]);  // active-low → positive logic
}

bool hal_read_button(void) {
    return !digitalRead(PIN_BUTTON);  // active-low → positive logic
}

uint16_t hal_read_dimmer_raw(void) {
    return (uint16_t)analogRead(PIN_DIMMER);
}

void hal_set_led_duty(hal_led_t led, uint16_t duty) {
    // Cathode-sink wiring: invert so duty 0=off, 4095=full bright.
    analogWrite(LED_PINS[led], 4095 - duty);
}

void hal_audio_play(uint8_t wav_id) {
    audio_glue_play(wav_id);
}

bool hal_audio_busy(void)  { return audio_glue_busy();  }
bool hal_audio_sd_ok(void) { return audio_glue_sd_ok(); }

void hal_log_write(const uint8_t *buf, size_t n) {
    Serial.write(buf, n);
}
