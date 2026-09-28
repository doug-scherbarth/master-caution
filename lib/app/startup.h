// lib/app/startup.h
// Power-up self-test sequence: color sweep → tone chime → green ACK flash.
// Blocks normal alarm processing until the pilot presses the ACK button.

#ifndef STARTUP_H
#define STARTUP_H

#include <stdint.h>
#include <stdbool.h>

// Starts the sequence.  Call after all modules are initialised.
//
// skip_lamp_test  — true when the reset cause was the hardware watchdog:
//                   lamp test is skipped and startup completes immediately so
//                   alarms are live without delay.
//
// cfg_fault       — true when the alarm config file was found but unreadable:
//                   triggers the CH_FAULT (4 Hz blue) warning in the sequence
//                   to alert the pilot that polarity/pull may be wrong.
void startup_init(uint32_t now_ms, bool skip_lamp_test, bool cfg_fault);

bool startup_active(void);

// Accept ACK button press — only acts during the final ACK_WAIT phase.
void startup_on_button_press(uint32_t now_ms);

// Called by app_tick whenever any alarm is pending during startup.
// Ends the ACK_WAIT phase immediately so the alarm owns the LED.
// No-op outside ACK_WAIT.
void startup_on_alarm_pending(uint32_t now_ms);

void startup_tick(uint32_t now_ms);

#endif // STARTUP_H
