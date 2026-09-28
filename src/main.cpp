// src/main.cpp
// Thin Arduino-style shim. All real logic lives in lib/app/ as portable C.

#include <Arduino.h>
extern "C" {
#include "app.h"
}

extern "C" void setup(void) { app_init(); }

extern "C" void loop(void) {
    app_tick();
#ifdef MC_DEBUG_HANG
    // Build with -DMC_DEBUG_HANG only. Send 'H' over serial to trigger a
    // controlled hang and verify watchdog reset behaviour (spec §10).
    if (Serial.available() && Serial.read() == 'H') {
        Serial.println("HANG triggered -- waiting for watchdog");
        for (;;) {}
    }
#endif
}
