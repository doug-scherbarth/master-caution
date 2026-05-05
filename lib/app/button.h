// lib/app/button.h
//
// Master caution pushbutton (spec §7.5). 50ms qualification.
// Short press: fires on the qualified rising edge — acks active alarms.
// Long press:  fires once after 3s continuous hold — triggers self-test.

#ifndef BUTTON_H
#define BUTTON_H

#include "types.h"

#define BUTTON_LONG_PRESS_MS  3000u

void button_init(void);

// Pass the polarity-corrected raw button state (true = physically pressed).
void button_tick(uint32_t now_ms, bool raw_pressed);

// Returns true exactly once per qualified press edge, then clears.
bool button_consume_press(void);

// Returns true exactly once after button held for BUTTON_LONG_PRESS_MS, then clears.
bool button_consume_long_press(void);

#endif // BUTTON_H
