// test/test_app/test_app.c
// App-level integration tests: app_init()/app_tick() against a time-aware
// HAL mock. These cover the wiring between modules, which no per-module
// suite exercises.
//
// Contract (TASKS.md, Tasks 1, 2, 4, 5):
//   - The alarm engine is never frozen by startup or test mode.
//   - A pending alarm pre-empts SS_ACK_WAIT (startup ends; alarm owns the LED).
//   - Startup tones are never interleaved with alarm audio: queued alarm audio
//     plays after the startup/test-mode tones finish.
//   - Normal dimmer use never changes the lighting config.
//   - Knob at zero keeps an acknowledged alarm at its (low) floor.
//
// Requires the Task 1 HAL change (tri-state hal_sd_read_file) to compile.

#include <unity.h>
#include "app.h"
#include "startup.h"
#include "alarm_engine.h"
#include "alarm_table.h"
#include "channel_table.h"
#include "pixel_lighting.h"
#include "hal.h"

extern uint32_t g_mock_now;
extern uint16_t mock_led_duty[4];
extern uint8_t  mock_play_log[];
extern int      mock_play_count;
extern int      mock_eeprom_puts;
void hal_mock_reset(void);
void hal_mock_set_alarm(uint8_t ch, bool v);
void hal_mock_set_button(bool pressed);
void hal_mock_set_adc(hal_adc_ch_t ch, uint16_t v);
void hal_mock_set_reset_cause(hal_reset_cause_t c);
void hal_mock_set_lights_cfg(const char *text);
bool mock_played(uint8_t wav);

#define TICK_MS        5u
#define BUS_12V     3039u
#define TONE_LO       13u
#define TONE_HI       15u
#define ACKED_FLOOR  614u    // 15 % of 4095 -- acknowledged floor must not exceed this

static const char *TWO_CONFIGS =
    "[global]\ntotal_pixels = 10\n"
    "[config]\nname = a\nstart = 0\ncount = 10\n"
    "[config]\nname = b\nstart = 0\ncount = 10\nr = 255\ng = 0\nb = 0\n";

static void run_until(uint32_t t_end) {
    while (g_mock_now < t_end) { app_tick(); g_mock_now += TICK_MS; }
}

static int index_of_play(uint8_t wav) {
    for (int i = 0; i < mock_play_count; i++) if (mock_play_log[i] == wav) return i;
    return -1;
}

void setUp(void) {
    hal_mock_reset();
    hal_mock_set_adc(HAL_ADC_BUS_SENSE, BUS_12V);
    hal_mock_set_adc(HAL_ADC_DIM_IN,    1500u);
}
void tearDown(void) {}

// --- Startup must not gate the alarm engine ----------------------------------

void test_por_alarm_present_at_boot_goes_live_without_press(void) {
    hal_mock_set_alarm(CH_CO_DETECT, true);
    app_init();
    run_until(20000);
    TEST_ASSERT_TRUE(mock_played(WAV_CO));
    TEST_ASSERT_NOT_EQUAL(AS_INACTIVE, alarm_engine_runtime(0)->state);
}

void test_alarm_during_ack_wait_preempts_startup(void) {
    app_init();
    run_until(5000);
    TEST_ASSERT_TRUE(startup_active());          // sanity: sitting in ACK_WAIT
    hal_mock_set_alarm(CH_CO_DETECT, true);
    run_until(7000);                              // well before the 10 s ACK timeout
    TEST_ASSERT_TRUE(mock_played(WAV_CO));
    TEST_ASSERT_FALSE(startup_active());
}

void test_startup_tones_not_interleaved_with_alarm_audio(void) {
    hal_mock_set_alarm(CH_CO_DETECT, true);
    app_init();
    run_until(20000);
    int lo = index_of_play(TONE_LO), hi = index_of_play(TONE_HI), co = index_of_play(WAV_CO);
    TEST_ASSERT_TRUE(lo >= 0 && hi >= 0 && co >= 0);
    TEST_ASSERT_EQUAL_INT(lo + 2, hi);            // 13,14,15 consecutive
    TEST_ASSERT_TRUE(co > hi);
}

void test_watchdog_boot_alarm_live_within_debounce(void) {     // guard
    hal_mock_set_reset_cause(HAL_RESET_WATCHDOG);
    hal_mock_set_alarm(CH_CO_DETECT, true);
    app_init();
    run_until(1000);
    TEST_ASSERT_TRUE(mock_played(WAV_CO));
}

void test_alarm_during_test_mode_announced_after(void) {       // guard
    hal_mock_set_reset_cause(HAL_RESET_WATCHDOG);
    app_init();
    run_until(500);
    hal_mock_set_button(true);  run_until(3700);  // 3.2 s hold -> test mode
    hal_mock_set_button(false);
    hal_mock_set_alarm(CH_CO_DETECT, true);
    run_until(15000);
    TEST_ASSERT_TRUE(mock_played(TONE_LO));       // test mode actually ran
    TEST_ASSERT_TRUE(mock_played(WAV_CO));
}

// --- Dimmer / lighting wiring -------------------------------------------------

void test_parked_low_knob_then_raise_keeps_lighting_config(void) {
    hal_mock_set_reset_cause(HAL_RESET_WATCHDOG);
    hal_mock_set_lights_cfg(TWO_CONFIGS);
    hal_mock_set_adc(HAL_ADC_DIM_IN, 304u);       // ~10 %: night
    app_init();
    run_until(60000);
    for (uint32_t k = 0; k <= 100u; k++) {        // raise to ~50 % over 10 s
        hal_mock_set_adc(HAL_ADC_DIM_IN, (uint16_t)(304u + (1520u - 304u) * k / 100u));
        run_until(g_mock_now + 100u);
    }
    TEST_ASSERT_EQUAL_UINT8(0, pixel_lighting_get_config());
    TEST_ASSERT_EQUAL_INT(0, mock_eeprom_puts);
}

void test_knob_at_zero_keeps_acked_alarm_at_floor(void) {
    hal_mock_set_reset_cause(HAL_RESET_WATCHDOG);
    hal_mock_set_adc(HAL_ADC_DIM_IN, 0u);         // cabin lights fully off
    hal_mock_set_alarm(CH_CO_DETECT, true);
    app_init();
    run_until(1000);
    hal_mock_set_button(true);  run_until(1100);  // acknowledge
    hal_mock_set_button(false);
    run_until(15000);                             // well past any 5 s hold-off
    TEST_ASSERT_TRUE(mock_led_duty[HAL_LED_RED] > 0u);
    TEST_ASSERT_TRUE(mock_led_duty[HAL_LED_RED] <= ACKED_FLOOR);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_por_alarm_present_at_boot_goes_live_without_press);
    RUN_TEST(test_alarm_during_ack_wait_preempts_startup);
    RUN_TEST(test_startup_tones_not_interleaved_with_alarm_audio);
    RUN_TEST(test_watchdog_boot_alarm_live_within_debounce);
    RUN_TEST(test_alarm_during_test_mode_announced_after);
    RUN_TEST(test_parked_low_knob_then_raise_keeps_lighting_config);
    RUN_TEST(test_knob_at_zero_keeps_acked_alarm_at_floor);
    return UNITY_END();
}
