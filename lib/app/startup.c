// lib/app/startup.c
#include "startup.h"
#include "hal.h"

// Sine-tone WAV IDs on SD card: 13.WAV, 14.WAV, 15.WAV
#define WAV_TONE_LO   13   // 440 Hz
#define WAV_TONE_MID  14   // 880 Hz
#define WAV_TONE_HI   15   // 1320 Hz

typedef enum {
    SS_RED = 0,
    SS_GREEN,
    SS_BLUE,
    SS_WHITE,
    SS_TONE_LO,
    SS_TONE_MID,
    SS_TONE_HI,
    SS_SD_ERROR,   // fast red flash for SD_ERR_DURATION_MS then → ACK_WAIT
    SS_ACK_WAIT,
    SS_DONE,
} ss_state_t;

#define LED_STEP_MS          500u
#define WHITE_STEP_MS        400u
#define FLASH_HALF_MS        250u   // 2 Hz green ACK flash
#define SD_ERR_FLASH_HALF_MS 100u   // 5 Hz red error flash
#define SD_ERR_DURATION_MS  5000u

static ss_state_t g_state;
static uint32_t   g_phase_start;
static uint32_t   g_flash_last;
static bool       g_flash_on;
static bool       g_sd_ok;

static void rgb(uint16_t r, uint16_t g, uint16_t b) {
    hal_set_led_duty(HAL_LED_RED,   r);
    hal_set_led_duty(HAL_LED_GREEN, g);
    hal_set_led_duty(HAL_LED_BLUE,  b);
}

static void enter(ss_state_t s, uint32_t now_ms) {
    g_state = s;
    switch (s) {
    case SS_RED:      rgb(4095, 0,    0);    g_phase_start = now_ms; break;
    case SS_GREEN:    rgb(0,    4095, 0);    g_phase_start = now_ms; break;
    case SS_BLUE:     rgb(0,    0,    4095); g_phase_start = now_ms; break;
    case SS_WHITE:    rgb(4095, 4095, 4095); g_phase_start = now_ms; break;
    case SS_TONE_LO:  rgb(0, 0, 0); hal_audio_play(WAV_TONE_LO);  break;
    case SS_TONE_MID:               hal_audio_play(WAV_TONE_MID); break;
    case SS_TONE_HI:                hal_audio_play(WAV_TONE_HI);  break;
    case SS_SD_ERROR:
        g_flash_on    = true;
        g_flash_last  = now_ms;
        g_phase_start = now_ms;
        rgb(4095, 0, 0);
        break;
    case SS_ACK_WAIT:
        g_flash_on   = true;
        g_flash_last = now_ms;
        rgb(0, 4095, 0);
        break;
    case SS_DONE:
    default:
        rgb(0, 0, 0);
        g_state = SS_DONE;
        break;
    }
}

void startup_init(uint32_t now_ms) {
    g_flash_on   = false;
    g_flash_last = 0;
    g_sd_ok      = hal_audio_sd_ok();
    enter(SS_RED, now_ms);
}

bool startup_active(void) { return g_state != SS_DONE; }

void startup_on_button_press(uint32_t now_ms) {
    if (g_state == SS_ACK_WAIT) enter(SS_DONE, now_ms);
}

void startup_tick(uint32_t now_ms) {
    switch (g_state) {
    case SS_RED:      if ((uint32_t)(now_ms-g_phase_start) >= LED_STEP_MS)   enter(SS_GREEN,    now_ms); break;
    case SS_GREEN:    if ((uint32_t)(now_ms-g_phase_start) >= LED_STEP_MS)   enter(SS_BLUE,     now_ms); break;
    case SS_BLUE:     if ((uint32_t)(now_ms-g_phase_start) >= LED_STEP_MS)   enter(SS_WHITE,    now_ms); break;
    case SS_WHITE:
        if ((uint32_t)(now_ms - g_phase_start) >= WHITE_STEP_MS)
            enter(g_sd_ok ? SS_TONE_LO : SS_SD_ERROR, now_ms);
        break;
    case SS_TONE_LO:  if (!hal_audio_busy()) enter(SS_TONE_MID, now_ms); break;
    case SS_TONE_MID: if (!hal_audio_busy()) enter(SS_TONE_HI,  now_ms); break;
    case SS_TONE_HI:  if (!hal_audio_busy()) enter(SS_ACK_WAIT, now_ms); break;
    case SS_SD_ERROR:
        if ((uint32_t)(now_ms - g_flash_last) >= SD_ERR_FLASH_HALF_MS) {
            g_flash_on   = !g_flash_on;
            g_flash_last = now_ms;
            hal_set_led_duty(HAL_LED_RED, g_flash_on ? 4095 : 0);
        }
        if ((uint32_t)(now_ms - g_phase_start) >= SD_ERR_DURATION_MS)
            enter(SS_ACK_WAIT, now_ms);
        break;
    case SS_ACK_WAIT:
        if ((uint32_t)(now_ms - g_flash_last) >= FLASH_HALF_MS) {
            g_flash_on   = !g_flash_on;
            g_flash_last = now_ms;
            hal_set_led_duty(HAL_LED_GREEN, g_flash_on ? 4095 : 0);
        }
        break;
    case SS_DONE: break;
    }
}
