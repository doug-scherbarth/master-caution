// lib/app/startup.h
// Power-up self-test sequence: color sweep → tone chime → green ACK flash.
// Blocks normal alarm processing until the pilot presses the ACK button.

#ifndef STARTUP_H
#define STARTUP_H

#include <stdint.h>
#include <stdbool.h>

// Starts the sequence immediately. Call after hal_init().
void startup_init(uint32_t now_ms);

bool startup_active(void);

// Accept ACK button press — only acts during the final ACK_WAIT phase.
void startup_on_button_press(uint32_t now_ms);

void startup_tick(uint32_t now_ms);

#endif // STARTUP_H
