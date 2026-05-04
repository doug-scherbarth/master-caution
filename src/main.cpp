// src/main.cpp
// Thin Arduino-style shim. All real logic lives in lib/app/ as portable C.

extern "C" {
#include "app.h"
}

extern "C" void setup(void) { app_init(); }
extern "C" void loop(void)  { app_tick(); }
