// lib/app/app.c
// Cooperative scheduler — wires all portable modules together.
// HAL provides time, inputs, and outputs; this module never touches hardware directly.

#include "app.h"
#include "debouncer.h"
#include "alarm_engine.h"
#include "audio_queue.h"
#include "button.h"
#include "dimmer.h"
#include "led_controller.h"
#include "aux_led.h"
#include "ring_log.h"
#include "startup.h"
#include "test_mode.h"
#include "channel_table.h"
#include "light_cfg.h"
#include "audio_cfg.h"
#include "pixel_lighting.h"
#include "dimmer_gesture.h"
#include "eeprom_map.h"
#include "alarm_cfg.h"
#include "alarm_table.h"
#include "hal.h"

static bool           g_raw_ch[CHANNEL_COUNT];
static bool           g_deb_ch[CHANNEL_COUNT];
static light_cfg_t    g_light_cfg;
static alarm_cfg_t    g_alarm_cfg;
static bool           g_pg_ok = false; // track PG edge; starts false so first tick captures baseline

void app_init(void) {
    hal_init();

    // Check reset cause before anything else — log it after ring_log_init.
    hal_reset_cause_t reset_cause = hal_reset_cause();

    // Load per-channel alarm config from SD.  Configure alarm pins immediately after
    // so pol/pull matches the config before any reading starts.
    alarm_cfg_status_t alarm_status = alarm_cfg_load(&g_alarm_cfg);
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        hal_alarm_configure(i,
                            g_alarm_cfg.ch[i].active_high,
                            (hal_pull_t)g_alarm_cfg.ch[i].pull);
    }

    // Build debounce array from loaded config and pass to debouncer.
    uint16_t dbnc_ms[CHANNEL_COUNT];
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        dbnc_ms[i] = g_alarm_cfg.ch[i].debounce_ms;
    debouncer_init(dbnc_ms);

    // Load audio gain from SD and apply to the amplifier.
    audio_cfg_t audio_cfg;
    audio_cfg_load(&audio_cfg);
    hal_audio_set_gain(audio_cfg.gain);

    alarm_engine_init();
    // Apply any wav_id overrides from ALARMS.CFG; only direct alarms have a channel.
    for (uint8_t i = 0; i < ALARM_COUNT; i++) {
        if (ALARM_TABLE[i].kind == SRC_DIRECT) {
            uint8_t ch_idx = ALARM_TABLE[i].channel;
            if (g_alarm_cfg.ch[ch_idx].wav_id_override < WAV_COUNT)
                alarm_engine_set_wav_override(i, g_alarm_cfg.ch[ch_idx].wav_id_override);
        }
    }
    audio_queue_init();
    button_init();
    dimmer_init();
    led_controller_init();
    aux_led_init();
    ring_log_init();
    test_mode_init();

    // Log reset cause now that ring_log is ready.
    if (reset_cause == HAL_RESET_WATCHDOG)
        ring_log_fault(LOG_FAULT_WATCHDOG_RESET, hal_millis());
    if (alarm_status == ALARM_CFG_ERROR || alarm_status == ALARM_CFG_WARN)
        ring_log_fault(LOG_FAULT_ALARM_CFG, hal_millis());

    // Dump effective alarm config to USB serial for field diagnostics.
    alarm_cfg_dump(&g_alarm_cfg, alarm_status);

    // Skip lamp test on watchdog reset so alarms are live immediately.
    // Alarm config error or warnings surface through the CH_FAULT (blue flash) path.
    startup_init(hal_millis(),
                 reset_cause == HAL_RESET_WATCHDOG,
                 alarm_status == ALARM_CFG_ERROR || alarm_status == ALARM_CFG_WARN);

    // Lighting: load config from SD (or fall back to built-in), restore
    // saved config index from EEPROM, enable buck regulator.
    light_cfg_load(&g_light_cfg);
    uint8_t saved_idx = hal_eeprom_get(EEPROM_ADDR_LIGHT_CFG);
    pixel_lighting_init(&g_light_cfg, saved_idx);
    dimmer_gesture_init(g_light_cfg.gesture_low_pct,
                        g_light_cfg.gesture_high_pct,
                        g_light_cfg.gesture_timeout_ms);
    hal_lbuck_enable(true);

    // Arm watchdog last — after all init (including SD reads) is complete.
    hal_watchdog_enable(2000);
}

void app_tick(void) {
    uint32_t now = hal_millis();

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        g_raw_ch[i] = hal_read_alarm(i);
    }
    debouncer_tick(now, g_raw_ch, g_deb_ch);

    button_tick(now, hal_read_button());
    bool pressed      = button_consume_press();
    bool long_pressed = button_consume_long_press();

    dimmer_tick(now, hal_adc_read(HAL_ADC_DIM_IN), hal_adc_read(HAL_ADC_BUS_SENSE));
    // HAL_LED_AUX is not wired on the v1.4 board; hal_set_led_duty is a no-op for it.
    hal_set_led_duty(HAL_LED_AUX, aux_led_compute_duty(dimmer_get_norm_q12()));

    // Alarm engine always runs — startup and test mode only borrow the LED/audio.
    alarm_engine_tick(now, g_deb_ch);

    if (startup_active()) {
        if (pressed) startup_on_button_press(now);
        if (alarm_engine_any_pending_ack()) startup_on_alarm_pending(now);
        startup_tick(now);
    } else if (test_mode_active()) {
        test_mode_tick(now);
    } else {
        if (pressed)      alarm_engine_on_button_press(now);
        if (long_pressed) test_mode_trigger(now);
        audio_queue_tick(now);

        led_drive_t drive;
        led_controller_tick(now,
                            alarm_engine_max_active_severity(),
                            alarm_engine_any_pending_ack(),
                            dimmer_get_norm_q12(),
                            g_light_cfg.mc_floor_ack_q12,
                            g_light_cfg.mc_floor_pending_q12,
                            &drive);
        hal_set_led_duty(HAL_LED_RED,   drive.red);
        hal_set_led_duty(HAL_LED_GREEN, drive.green);
        hal_set_led_duty(HAL_LED_BLUE,  drive.blue);
    }

    ring_log_tick();

    uint8_t dim_ratio = dimmer_get_ratio_u8();
    if (dimmer_gesture_tick(now, dim_ratio)) {
        uint8_t next = (uint8_t)((pixel_lighting_get_config() + 1u) % g_light_cfg.n_configs);
        pixel_lighting_set_config(next);
        hal_eeprom_put(EEPROM_ADDR_LIGHT_CFG, next);
    }

    pixel_lighting_tick(now, dim_ratio);

    // Monitor lighting buck PG; log once on falling edge (good → faulted).
    bool pg_now = hal_lbuck_pg();
    if (!pg_now && g_pg_ok) ring_log_fault(LOG_FAULT_LBUCK_PG, now);
    g_pg_ok = pg_now;

    // Kick watchdog at the END of a complete loop pass.
    // Kicking from here (not an ISR) ensures the CPU is completing full cycles.
    hal_watchdog_kick();
}
