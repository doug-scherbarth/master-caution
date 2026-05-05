// test/test_startup/hal_mock.c

#include "hal.h"
#include "channel_table.h"
#include <stddef.h>
#include <string.h>

uint16_t hal_led_duty[4];
int      hal_play_calls     = 0;
uint8_t  hal_play_log[16];
int      hal_play_log_count = 0;

static bool g_busy   = false;
static bool g_sd_ok  = true;
static bool g_alarm[CHANNEL_COUNT];

void hal_mock_reset(void) {
    for (int i = 0; i < 4; i++) hal_led_duty[i] = 0;
    hal_play_calls     = 0;
    hal_play_log_count = 0;
    g_busy             = false;
    g_sd_ok            = true;
    memset(g_alarm, 0, sizeof(g_alarm));
}

void hal_mock_set_busy(bool busy)              { g_busy = busy; }
void hal_mock_set_sd_ok(bool ok)               { g_sd_ok = ok; }
void hal_mock_set_alarm(uint8_t ch, bool val)  { if (ch < CHANNEL_COUNT) g_alarm[ch] = val; }

void hal_set_led_duty(hal_led_t led, uint16_t duty) { hal_led_duty[led] = duty; }

void hal_audio_play(uint8_t wav_id) {
    hal_play_calls++;
    if (hal_play_log_count < 16) hal_play_log[hal_play_log_count++] = wav_id;
    g_busy = true;
}

bool     hal_audio_busy(void)                      { return g_busy;  }
bool     hal_audio_sd_ok(void)                     { return g_sd_ok; }
void     hal_init(void)                            {}
uint32_t hal_millis(void)                          { return 0; }
bool     hal_read_alarm(uint8_t ch)                { return (ch < CHANNEL_COUNT) ? g_alarm[ch] : false; }
bool     hal_read_button(void)                     { return false; }
uint16_t hal_read_dimmer_raw(void)                 { return 0; }
void     hal_log_write(const uint8_t *b, size_t n) { (void)b; (void)n; }
