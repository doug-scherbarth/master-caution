// lib/app/ring_log.c
#include "ring_log.h"
#include "alarm_table.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

#define RING_LOG_CAPACITY  32   // power of 2 — wrap is index & MASK
#define RING_LOG_MASK      (RING_LOG_CAPACITY - 1)

typedef struct {
    uint32_t    ts_ms;
    log_event_t event;
    uint8_t     alarm_idx;
} record_t;

static record_t  g_buf[RING_LOG_CAPACITY];
static uint8_t   g_head;          // write index, advances on producer
static uint8_t   g_tail;          // read index, advances on consumer
static uint32_t  g_dropped;

static const char *event_name(log_event_t e) {
    switch (e) {
    case LOG_ALARM_ASSERT:       return "ASSERT";
    case LOG_ALARM_ACK:          return "ACK";
    case LOG_ALARM_CLEAR:        return "CLEAR";
    case LOG_ALARM_CLEAR_NO_ACK: return "CLEAR_NO_ACK";
    default:                     return "?";
    }
}

static uint8_t pending(void) {
    return (uint8_t)((g_head - g_tail) & RING_LOG_MASK);
}

void ring_log_init(void) {
    memset(g_buf, 0, sizeof(g_buf));
    g_head    = 0;
    g_tail    = 0;
    g_dropped = 0;
}

void ring_log_alarm(log_event_t event, uint8_t alarm_idx, uint32_t now_ms) {
    // Full when next write would collide with tail.
    if (((uint8_t)(g_head + 1) & RING_LOG_MASK) == g_tail) {
        g_dropped++;
        return;
    }
    g_buf[g_head].ts_ms     = now_ms;
    g_buf[g_head].event     = event;
    g_buf[g_head].alarm_idx = alarm_idx;
    g_head = (uint8_t)(g_head + 1) & RING_LOG_MASK;
}

void ring_log_tick(void) {
    if (g_head == g_tail) return;          // empty

    const record_t *r = &g_buf[g_tail];
    const char *name  = (r->alarm_idx < ALARM_COUNT)
                          ? ALARM_TABLE[r->alarm_idx].name : "?";

    char line[64];
    int n = snprintf(line, sizeof(line), "[%lu] %s %s\n",
                     (unsigned long)r->ts_ms, name, event_name(r->event));
    if (n > 0) {
        if (n > (int)(sizeof(line) - 1)) n = (int)(sizeof(line) - 1);
        hal_log_write((const uint8_t *)line, (size_t)n);
    }
    g_tail = (uint8_t)(g_tail + 1) & RING_LOG_MASK;
}

uint32_t ring_log_dropped_count(void) { return g_dropped; }
uint8_t  ring_log_pending_count(void) { return pending(); }
