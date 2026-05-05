// lib/app/alarm_engine.c
#include "alarm_engine.h"
#include "alarm_table.h"
#include "audio_queue.h"
#include "ring_log.h"

static alarm_runtime_t g_rt[ALARM_COUNT_MAX];

static bool eval_alarm(const alarm_descriptor_t *a, const bool *ch) {
    if (a->kind == SRC_DIRECT) return ch[a->channel];
    return (a->composite != NULL) && a->composite(ch);
}

static void enter_state(uint8_t i, alarm_state_t s, uint32_t now_ms) {
    g_rt[i].state            = s;
    g_rt[i].entered_state_ms = now_ms;
}

void alarm_engine_init(void) {
    for (uint8_t i = 0; i < ALARM_COUNT_MAX; i++) {
        g_rt[i].state = AS_INACTIVE;
        g_rt[i].entered_state_ms = 0;
    }
}

void alarm_engine_tick(uint32_t now_ms, const bool *ch) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        const alarm_descriptor_t *a = &ALARM_TABLE[i];
        const bool asserted = eval_alarm(a, ch);

        switch (g_rt[i].state) {
        case AS_INACTIVE:
            if (asserted) {
                enter_state(i, AS_PENDING_ACK, now_ms);
                audio_queue_push(a->wav_id, a->severity);
                ring_log_alarm(LOG_ALARM_ASSERT, i, now_ms);
            }
            break;

        case AS_PENDING_ACK:
            if (!asserted) {
                enter_state(i, AS_INACTIVE, now_ms);
                ring_log_alarm(LOG_ALARM_CLEAR_NO_ACK, i, now_ms);
            }
            break;

        case AS_ACKNOWLEDGED:
            if (!asserted) {
                enter_state(i, AS_INACTIVE, now_ms);
                ring_log_alarm(LOG_ALARM_CLEAR, i, now_ms);
            }
            break;
        }
    }
}

void alarm_engine_on_button_press(uint32_t now_ms) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (g_rt[i].state == AS_PENDING_ACK) {
            enter_state(i, AS_ACKNOWLEDGED, now_ms);
            ring_log_alarm(LOG_ALARM_ACK, i, now_ms);
        }
    }
}

severity_t alarm_engine_max_active_severity(void) {
    severity_t max = SEV_NONE;
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (g_rt[i].state != AS_INACTIVE) {
            const severity_t s = ALARM_TABLE[i].severity;
            if (s > max) max = s;
        }
    }
    return max;
}

bool alarm_engine_any_pending_ack(void) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (g_rt[i].state == AS_PENDING_ACK) return true;
    }
    return false;
}

const alarm_runtime_t *alarm_engine_runtime(uint8_t alarm_idx) {
    return (alarm_idx < ALARM_COUNT) ? &g_rt[alarm_idx] : (alarm_runtime_t *)0;
}
