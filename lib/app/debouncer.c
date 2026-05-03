// lib/app/debouncer.c
#include "debouncer.h"
#include "channel_table.h"

typedef struct {
    bool     debounced;              // stable output
    bool     in_transition;          // raw differs from debounced; timer running
    uint32_t transition_started_ms;
} dbnc_state_t;

static dbnc_state_t g_ch[CHANNEL_COUNT];

void debouncer_init(void) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        g_ch[i].debounced             = false;
        g_ch[i].in_transition         = false;
        g_ch[i].transition_started_ms = 0;
    }
}

void debouncer_tick(uint32_t now_ms, const bool *raw, bool *out) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        dbnc_state_t *s = &g_ch[i];
        const bool    r = raw[i];

        if (r == s->debounced) {
            // Raw matches stable output — nothing to qualify, kill any pending transition.
            s->in_transition = false;
        } else if (!s->in_transition) {
            // First tick the input differs — start the qualification timer.
            s->in_transition         = true;
            s->transition_started_ms = now_ms;
        } else if ((uint32_t)(now_ms - s->transition_started_ms)
                   >= CHANNEL_TABLE[i].debounce_ms) {
            // Qualification time elapsed with input still different — accept the new state.
            s->debounced     = r;
            s->in_transition = false;
        }
        out[i] = s->debounced;
    }
}
