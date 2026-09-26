// src/pintest/wstest.cpp
// WS2812B + dimmer bring-up — compiled with env:wstest only.
// Reads DIM_IN (pin 26) ratiometrically against BUS_SENSE (pin 27).
// Drives 4 pixels red; brightness tracks the dimmer pot 0–12 V input.
//
// Wiring:
//   DIM_IN    → Teensy pin 26 (A12) via 39k/10k divider (spec §6.2)
//   BUS_SENSE → Teensy pin 27 (A13) via identical divider
//
// USB serial prints ratio and brightness at 2 Hz for calibration.

#include <Arduino.h>
#include <WS2812Serial.h>

#define NUM_LEDS      4
#define PIN_DATA      29
#define PIN_DIM_IN    26
#define PIN_BUS_SENSE 27

// Dead bands match production dimmer.c (3% / 97% of 4095)
#define DEADBAND_LOW  123u
#define DEADBAND_HIGH 3972u

static byte drawingMemory[NUM_LEDS * 3];
DMAMEM static byte displayMemory[NUM_LEDS * 12];
static WS2812Serial leds(NUM_LEDS, displayMemory, drawingMemory, PIN_DATA, WS2812_GRB);

// Returns ratiometric dim level 0–4095.
static uint16_t read_dim_ratio(void) {
    uint16_t dim = (uint16_t)analogRead(PIN_DIM_IN);
    uint16_t bus = (uint16_t)analogRead(PIN_BUS_SENSE);
    if (bus == 0) return 0;
    uint32_t r = (uint32_t)dim * 4096u / bus;
    return (r > 4095u) ? 4095u : (uint16_t)r;
}

// Maps ratio 0–4095 to pixel brightness 0–255, with dead bands.
static uint8_t ratio_to_brightness(uint16_t ratio) {
    if (ratio <= DEADBAND_LOW)  return 0;
    if (ratio >= DEADBAND_HIGH) return 255;
    // Linear map from [DEADBAND_LOW..DEADBAND_HIGH] → [0..255]
    uint32_t span = DEADBAND_HIGH - DEADBAND_LOW;
    return (uint8_t)(((uint32_t)(ratio - DEADBAND_LOW) * 255u) / span);
}

void setup(void) {
    Serial.begin(115200);
    analogReadResolution(12);
    leds.begin();
}

void loop(void) {
    static uint32_t last_print_ms = 0;

    uint16_t ratio      = read_dim_ratio();
    uint8_t  brightness = ratio_to_brightness(ratio);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds.setPixel(i, brightness, 0, 0);   // red channel only
    }
    leds.show();

    if ((uint32_t)(millis() - last_print_ms) >= 500) {
        uint16_t dim = (uint16_t)analogRead(PIN_DIM_IN);
        uint16_t bus = (uint16_t)analogRead(PIN_BUS_SENSE);
        Serial.printf("DIM_IN=%4u  BUS_SENSE=%4u  ratio=%4u  brightness=%3u\n",
                      dim, bus, ratio, brightness);
        last_print_ms = millis();
    }

    delay(20);  // 50 Hz update
}
