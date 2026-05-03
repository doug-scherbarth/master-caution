// lib/app/button.h
//
// Master caution pushbutton (spec §7.5). 50ms qualification. Emits a
// single press event on the qualified press edge — caller polls
// button_consume_press() once per tick and forwards true to the
// alarm engine.

#ifndef BUTTON_H
#define BUTTON_H

#include "types.h"

void button_init(void);

// Pass the polarity-corrected raw button state (true = physically pressed).
void button_tick(uint32_t now_ms, bool raw_pressed);

// Returns true exactly once per qualified press edge, then clears.
bool button_consume_press(void);

#endif // BUTTON_H
