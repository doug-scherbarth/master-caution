// lib/app/audio_queue.c
#include "audio_queue.h"
#include "hal.h"
#include <string.h>

#define AUDIO_QUEUE_CAPACITY  16

typedef struct {
    uint8_t    wav_id;
    severity_t severity;
    uint32_t   seq;        // FIFO tiebreaker — lower seq wins among equal severity
} entry_t;

static entry_t  g_q[AUDIO_QUEUE_CAPACITY];
static uint8_t  g_count;
static uint32_t g_next_seq;
static bool     g_have_now_playing;
static uint8_t  g_now_playing_wav;
static uint32_t g_dropped_full;
static uint32_t g_dropped_dup;

// --- helpers --------------------------------------------------------

static bool is_already_present(uint8_t wav_id) {
    if (g_have_now_playing && g_now_playing_wav == wav_id) return true;
    for (uint8_t i = 0; i < g_count; i++) {
        if (g_q[i].wav_id == wav_id) return true;
    }
    return false;
}

// Find the index of the highest-priority entry: max severity, then min seq.
static uint8_t find_best(void) {
    uint8_t best = 0;
    for (uint8_t i = 1; i < g_count; i++) {
        if (g_q[i].severity >  g_q[best].severity) { best = i; continue; }
        if (g_q[i].severity == g_q[best].severity &&
            g_q[i].seq      <  g_q[best].seq)      { best = i; }
    }
    return best;
}

static void remove_at(uint8_t idx) {
    // Order of remaining entries doesn't matter — selection scans them all.
    g_q[idx] = g_q[g_count - 1];
    g_count--;
}

// --- public API -----------------------------------------------------

void audio_queue_init(void) {
    memset(g_q, 0, sizeof(g_q));
    g_count            = 0;
    g_next_seq         = 0;
    g_have_now_playing = false;
    g_now_playing_wav  = 0;
    g_dropped_full     = 0;
    g_dropped_dup      = 0;
}

void audio_queue_push(uint8_t wav_id, severity_t severity) {
    if (is_already_present(wav_id)) {
        g_dropped_dup++;
        return;
    }
    if (g_count >= AUDIO_QUEUE_CAPACITY) {
        g_dropped_full++;
        return;
    }
    g_q[g_count].wav_id   = wav_id;
    g_q[g_count].severity = severity;
    g_q[g_count].seq      = g_next_seq++;
    g_count++;
}

void audio_queue_tick(uint32_t now_ms) {
    (void)now_ms;

    // If we believed something was playing, check whether it's done.
    if (g_have_now_playing && !hal_audio_busy()) {
        g_have_now_playing = false;
    }

    // Idle and have work? Dispatch the highest-priority entry.
    if (!g_have_now_playing && g_count > 0) {
        uint8_t best = find_best();
        g_now_playing_wav  = g_q[best].wav_id;
        g_have_now_playing = true;
        hal_audio_play(g_q[best].wav_id);
        remove_at(best);
    }
}

uint32_t audio_queue_dropped_full_count(void) { return g_dropped_full; }
uint32_t audio_queue_dropped_dup_count(void)  { return g_dropped_dup;  }
