// src/main.cpp
// Thin Arduino-style shim. All real logic lives in lib/app/ as portable C.

extern "C" {
    void app_init(void);
    void app_tick(void);
}

void setup(void) { app_init(); }
void loop(void)  { app_tick(); }
