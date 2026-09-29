// lib/app/alarm_engine.c
#include "alarm_engine.h"
#include "alarm_table.h"
#include "audio_queue.h"
#include "ring_log.h"

static alarm_runtime_t g_rt[ALARM_COUNT_MAX];
static uint8_t         g_wav_id[ALARM_COUNT_MAX]; // 0xFF = use ALARM_TABLE default

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
        g_rt[i].state            = AS_INACTIVE;
        g_rt[i].entered_state_ms = 0;
        g_wav_id[i]              = 0xFF;
    }
}

void alarm_engine_set_wav_override(uint8_t alarm_idx, uint8_t wav_id) {
    if (alarm_idx < ALARM_COUNT_MAX && wav_id < WAV_COUNT)
        g_wav_id[alarm_idx] = wav_id;
}

void alarm_engine_tick(uint32_t now_ms, const bool *ch) {
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        const alarm_descriptor_t *a = &ALARM_TABLE[i];
        const bool asserted = eval_alarm(a, ch);

        switch (g_rt[i].state) {
        case AS_INACTIVE:
            if (asserted) {
                enter_state(i, AS_PENDING_ACK, now_ms);
                uint8_t wid = (g_wav_id[i] < WAV_COUNT) ? g_wav_id[i] : a->wav_id;
                audio_queue_push(wid, a->severity);
                ring_log_alarm(LOG_ALARM_ASSERT, i, now_ms);
            }
            break;

        case AS_PENDING_ACK:
            if (!asserted) {
                enter_state(i, AS_PENDING_ACK_CLEARED, now_ms);
                ring_log_alarm(LOG_ALARM_CLEAR_NO_ACK, i, now_ms);
            }
            break;

        case AS_PENDING_ACK_CLEARED:
            if (asserted) {
                // Condition re-asserted before ack — return to PENDING_ACK silently.
                // Audio already announced this onset; repeating adds workload without info.
                enter_state(i, AS_PENDING_ACK, now_ms);
                ring_log_alarm(LOG_ALARM_REASSERT, i, now_ms);
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
        } else if (g_rt[i].state == AS_PENDING_ACK_CLEARED) {
            // Condition already gone — no ACKNOWLEDGED phase needed.
            enter_state(i, AS_INACTIVE, now_ms);
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
        if (g_rt[i].state == AS_PENDING_ACK ||
            g_rt[i].state == AS_PENDING_ACK_CLEARED) return true;
    }
    return false;
}

const alarm_runtime_t *alarm_engine_runtime(uint8_t alarm_idx) {
    return (alarm_idx < ALARM_COUNT) ? &g_rt[alarm_idx] : (alarm_runtime_t *)0;
}
