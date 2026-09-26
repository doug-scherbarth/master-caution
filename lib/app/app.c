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
#include "pixel_lighting.h"
#include "dimmer_gesture.h"
#include "eeprom_map.h"
#include "hal.h"

static bool        g_raw_ch[CHANNEL_COUNT];
static bool        g_deb_ch[CHANNEL_COUNT];
static light_cfg_t g_light_cfg;

void app_init(void) {
    hal_init();
    debouncer_init();
    alarm_engine_init();
    audio_queue_init();
    button_init();
    dimmer_init();
    led_controller_init();
    aux_led_init();
    ring_log_init();
    test_mode_init();
    startup_init(hal_millis());

    // Lighting: load config from SD (or fall back to built-in), restore
    // saved config index from EEPROM, enable buck regulator.
    light_cfg_load(&g_light_cfg);
    uint8_t saved_idx = hal_eeprom_get(EEPROM_ADDR_LIGHT_CFG);
    pixel_lighting_init(&g_light_cfg, saved_idx);
    dimmer_gesture_init(g_light_cfg.gesture_low_pct,
                        g_light_cfg.gesture_high_pct,
                        g_light_cfg.gesture_timeout_ms);
    hal_lbuck_enable(true);
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

    if (startup_active()) {
        if (pressed) startup_on_button_press(now);
        startup_tick(now);
    } else if (test_mode_active()) {
        test_mode_tick(now);
    } else {
        if (pressed)      alarm_engine_on_button_press(now);
        if (long_pressed) test_mode_trigger(now);
        alarm_engine_tick(now, g_deb_ch);
        audio_queue_tick(now);

        led_drive_t drive;
        led_controller_tick(now,
                            alarm_engine_max_active_severity(),
                            alarm_engine_any_pending_ack(),
                            dimmer_get_norm_q12(),
                            &drive);
        hal_set_led_duty(HAL_LED_RED,   drive.red);
        hal_set_led_duty(HAL_LED_GREEN, drive.green);
        hal_set_led_duty(HAL_LED_BLUE,  drive.blue);
    }

    ring_log_tick();

    // Dimmer gesture: quick dip (<20%) or bump (>80%) advances lighting config
    uint8_t dim_ratio = (uint8_t)(dimmer_get_norm_q12() >> 4);
    if (dimmer_gesture_tick(now, dim_ratio)) {
        uint8_t next = (uint8_t)((pixel_lighting_get_config() + 1u) % g_light_cfg.n_configs);
        pixel_lighting_set_config(next);
        hal_eeprom_put(EEPROM_ADDR_LIGHT_CFG, next);
    }

    pixel_lighting_tick();
}
