// lib/app/test_mode.c
#include "test_mode.h"
#include "startup_tones.h"
#include "hal.h"

typedef enum {
    TM_IDLE = 0,
    TM_LED_RED,
    TM_LED_GREEN,
    TM_LED_BLUE,
    TM_TONE_LO,
    TM_TONE_MID,
    TM_TONE_HI,
} tm_state_t;

#define LED_STEP_MS 500u

static tm_state_t g_state      = TM_IDLE;
static uint32_t   g_phase_start = 0;

static void enter(tm_state_t s, uint32_t now_ms) {
    g_state = s;
    switch (s) {
    case TM_LED_RED:    rgb(4095, 0,    0);    g_phase_start = now_ms; break;
    case TM_LED_GREEN:  rgb(0,    4095, 0);    g_phase_start = now_ms; break;
    case TM_LED_BLUE:   rgb(0,    0,    4095); g_phase_start = now_ms; break;
    case TM_TONE_LO:    rgb(0, 0, 0); hal_audio_play(STARTUP_WAV_LO);  break;
    case TM_TONE_MID:                  hal_audio_play(STARTUP_WAV_MID); break;
    case TM_TONE_HI:                   hal_audio_play(STARTUP_WAV_HI);  break;
    case TM_IDLE:
    default:            rgb(0, 0, 0); g_state = TM_IDLE;               break;
    }
}

void test_mode_init(void)  { g_state = TM_IDLE; }
bool test_mode_active(void) { return g_state != TM_IDLE; }

void test_mode_trigger(uint32_t now_ms) {
    if (g_state == TM_IDLE) enter(TM_LED_RED, now_ms);
}

void test_mode_tick(uint32_t now_ms) {
    switch (g_state) {
    case TM_IDLE:     break;
    case TM_LED_RED:  if ((uint32_t)(now_ms - g_phase_start) >= LED_STEP_MS)  enter(TM_LED_GREEN, now_ms); break;
    case TM_LED_GREEN:if ((uint32_t)(now_ms - g_phase_start) >= LED_STEP_MS)  enter(TM_LED_BLUE,  now_ms); break;
    case TM_LED_BLUE: if ((uint32_t)(now_ms - g_phase_start) >= LED_STEP_MS)  enter(TM_TONE_LO,   now_ms); break;
    case TM_TONE_LO:  if (!hal_audio_busy())                                   enter(TM_TONE_MID,  now_ms); break;
    case TM_TONE_MID: if (!hal_audio_busy())                                   enter(TM_TONE_HI,   now_ms); break;
    case TM_TONE_HI:  if (!hal_audio_busy())                                   enter(TM_IDLE,      now_ms); break;
    }
}
