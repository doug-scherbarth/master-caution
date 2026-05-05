// lib/app/button.c
#include "button.h"
#include <string.h>

#define BUTTON_DEBOUNCE_MS  50

typedef struct {
    bool     debounced;            // qualified state (true = pressed)
    bool     in_transition;        // raw differs from debounced
    uint32_t transition_started_ms;
    bool     press_pending;        // short press edge, consumed by app
    uint32_t pressed_since_ms;     // when debounced last went true
    bool     long_press_pending;   // long press event, consumed by app
    bool     long_press_fired;     // prevents re-fire while held
} button_state_t;

static button_state_t g_b;

void button_init(void) {
    memset(&g_b, 0, sizeof(g_b));
}

void button_tick(uint32_t now_ms, bool raw_pressed) {
    if (raw_pressed == g_b.debounced) {
        g_b.in_transition = false;
        if (g_b.debounced && !g_b.long_press_fired) {
            if ((uint32_t)(now_ms - g_b.pressed_since_ms) >= BUTTON_LONG_PRESS_MS) {
                g_b.long_press_pending = true;
                g_b.long_press_fired   = true;
            }
        }
        return;
    }
    if (!g_b.in_transition) {
        g_b.in_transition         = true;
        g_b.transition_started_ms = now_ms;
        return;
    }
    if ((uint32_t)(now_ms - g_b.transition_started_ms) >= BUTTON_DEBOUNCE_MS) {
        g_b.debounced     = raw_pressed;
        g_b.in_transition = false;
        if (g_b.debounced) {
            g_b.press_pending      = true;
            g_b.pressed_since_ms   = now_ms;
            g_b.long_press_fired   = false;
        }
    }
}

bool button_consume_press(void) {
    if (g_b.press_pending) { g_b.press_pending = false; return true; }
    return false;
}

bool button_consume_long_press(void) {
    if (g_b.long_press_pending) { g_b.long_press_pending = false; return true; }
    return false;
}
