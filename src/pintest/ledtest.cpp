// src/pintest/ledtest.cpp
// LED output verification — compiled with env:ledtest only.
// PWM sweep: ramps each LED 0→full→0 over 2 s, one at a time, then repeats.

#include <Arduino.h>

#define PIN_LED_R   25
#define PIN_LED_G   24
#define PIN_LED_B   28

// N-FET drive: duty 0 = off, 4095 = full bright (HIGH = on)
static void set_duty(uint8_t pin, uint16_t duty) {
    analogWrite(pin, duty);
}

static void all_off(void) {
    set_duty(PIN_LED_R, 0);
    set_duty(PIN_LED_G, 0);
    set_duty(PIN_LED_B, 0);
}

// Ramp pin from 0 → 4095 → 0, total duration ~2 s
static void sweep(uint8_t pin) {
    for (int d = 0; d <= 4095; d += 4) {
        set_duty(pin, d);
        delay(1);
    }
    for (int d = 4095; d >= 0; d -= 4) {
        set_duty(pin, d);
        delay(1);
    }
}

void setup(void) {
    analogWriteResolution(12);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    all_off();
}

void loop(void) {
    sweep(PIN_LED_R);  all_off();  delay(300);
    sweep(PIN_LED_G);  all_off();  delay(300);
    sweep(PIN_LED_B);  all_off();  delay(300);
}
