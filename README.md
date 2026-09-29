# Master Caution Annunciator — Firmware

PlatformIO + Teensy 4.1 firmware for the v1.3 hardware spec.

## Build

```
pio run                   # default env: teensy41
pio run -t upload         # flash via teensy_loader_cli
pio run -e host           # native build for unit tests
pio test -e host          # run Unity tests on Linux
pio device monitor        # USB serial @ 115200
```

## Layout

```
src/
├── main.cpp              Arduino-style shim → app_init/app_tick
├── hal_teensy/           Real HAL: Teensy GPIO, ADC, I2S, SD card
└── hal_host/             Mock HAL for desktop unit tests
lib/
├── hal/hal.h             Pure-C HAL interface (polarity-corrected)
└── app/                  All portable logic (no Arduino headers)
    ├── app.{h,c}         Top-level init + cooperative scheduler
    ├── types.h
    ├── channel_table.{h,c}   15 physical inputs, per-channel debounce
    ├── alarm_table.{h,c}     13 pilot alarms, descriptor-driven
    ├── debouncer.{h,c}       Per-channel qualification timer
    ├── dimmer.{h,c}          ADC + IIR filter + fail-safe
    ├── button.{h,c}          Debounced press → ack event
    ├── alarm_engine.{h,c}    State machine + severity rollup
    ├── led_controller.{h,c}  Severity + flash + dimmer → 3× PWM
    ├── aux_led.{h,c}         Dimmer follower → pin 29 PWM
    ├── audio_queue.{h,c}     Priority queue of WAV requests
    └── ring_log.{h,c}        Timestamped event ring → USB serial
test/
├── test_debouncer/
├── test_alarm_engine/
└── test_dimmer/
```

## Architectural rules

1. **`lib/app/` is portable C.** No `<Arduino.h>`, no `<SD.h>`, no `digitalRead`. The only outward dependency is `hal.h`. This means every alarm-engine bug can be reproduced in a Unity test on the desktop.

2. **HAL returns positive logic.** Inputs are active-low at the connector; `hal_read_alarm()` and `hal_read_button()` invert before returning. Application code never sees polarity.

3. **Tables, not switch statements.** Adding an alarm = one row in `alarm_table.c`. Composite alarms (FLAPS_DEPLOYED && AIRSPEED_OVER_VFE) are a function pointer in the row, not a special case in the engine.

4. **Debounce belongs to the channel, not the alarm.** Each physical input has its own qualification time. Composite alarms are the AND of already-debounced signals — no extra timer needed.

5. **One C++ file in the tree.** `audio_glue.cpp` wraps the Teensy Audio library. Everything else is C.

6. **Cooperative scheduler.** No RTOS. `app_tick()` runs each module's tick at its own rate; the Teensy Audio library handles I2S in its own ISR independently.

## State machine (alarm engine)

```
              condition asserts
              [AUDIO queued]
 ┌──────────┐ ──────────────────► ┌─────────────────┐
 │ INACTIVE │                      │  PENDING_ACK    │
 └──────────┘                      │   (flashing)    │
      ▲  ▲                         └────────┬────────┘
      │  │                          cond    │ condition
      │  │          condition       re-     │ clears
      │  │          re-asserts    asserts   │
      │  │          [SILENT]        ▲       ▼
      │  │      button       ┌─────────────────────┐
      │  └─────── press ─── │  PENDING_ACK_CLEARED │
      │                      │      (flashing)      │
      │                      └──────────────────────┘
      │      condition
      │      cleared         ┌─────────────────┐
      └───────────────────── │  ACKNOWLEDGED   │
                              │    (steady)     │
                              └─────────────────┘
```

An unacknowledged alarm always latches: if the input de-asserts before the pilot presses the button, the alarm moves to `PENDING_ACK_CLEARED` and keeps flashing until acknowledged. This ensures every onset is consciously dismissed.

A re-assertion from `PENDING_ACK_CLEARED` returns to `PENDING_ACK` **silently** — the audio already announced this onset, and the button is already flashing. Audio fires only on the `INACTIVE → PENDING_ACK` transition (fresh onset from a cleared-and-acked state).

LED display is computed *from* state, not stored:
- `display_color = max_severity_among({PENDING_ACK ∪ PENDING_ACK_CLEARED ∪ ACKNOWLEDGED})`
- `flashing = any_alarm_in_{PENDING_ACK ∪ PENDING_ACK_CLEARED}`
- `duty = max(MC_FLOOR, dimmer_norm)`

Re-trigger (spec §4.6): a cleared-and-acked alarm returns to INACTIVE; the next assertion is a fresh `INACTIVE → PENDING_ACK` transition that fires audio again.
