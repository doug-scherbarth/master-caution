// src/hal_teensy/hal_teensy.cpp
// Teensy 4.1 HAL — spec v1.4 pin assignments.
//
// DB-15 #1 alarm inputs (active-high default; polarity set by hal_alarm_configure):
//   CH[ 0] CH_CO_DETECT        → pin  0
//   CH[ 1] CH_L_MAG_FAIL       → pin  1
//   CH[ 2] CH_R_MAG_FAIL       → pin  5
//   CH[ 3] CH_OIL_PRESS_LOW    → pin  6
//   CH[ 4] CH_CHT_OVERTEMP     → pin  8
//   CH[ 5] CH_EGT_OVERTEMP     → pin  9
//   CH[ 6] CH_PRIM_ALT_FAIL    → pin 11
//   CH[ 7] CH_SEC_ALT_FAIL     → pin 12
//   CH[ 8] CH_PITOT_HEAT_FAIL  → pin 14
//   CH[ 9] CH_OIL_TEMP_HIGH    → pin 15
//   CH[10] CH_FUEL_PRESS_LOW   → pin 16
//   CH[11] CH_FLAPS_DEPLOYED   → pin 17
//   CH[12] CH_AIRSPEED_OVER_VFE→ pin 22
//   CH[13] CH_BOOST_PUMP_ON    → pin 23
//
// Button SW_MC (active-low, internal pull-up) → pin 18
//
// Dimmer (ratiometric, §6.2):
//   DIM_IN    → pin 26 (A12)
//   BUS_SENSE → pin 27 (A13)
//
// Button LED (cathode-sink, §9.4):
//   Red   → pin 25   Blue  → pin 28
//   Green → pin 24   AUX   → not wired (HAL_LED_AUX writes are ignored)
//
// I2S (managed by Audio library):
//   pin  7 = I2S TX data (DIN → PCM5102)
//   pin 20 = I2S LRCLK
//   pin 21 = I2S BCLK
//
// WS2812 data → pin 29 (Serial7 TX, DMA via WS2812Serial).
// Lighting buck EN → pin 30 (N-FET, active-low to buck), PG → pin 31 (open-drain, INPUT_PULLDOWN).

#include <Arduino.h>
#include <WS2812Serial.h>
#include <EEPROM.h>
#include <SD.h>
#include <imxrt.h>
#include "hal.h"
#include "audio_glue.h"

// --- Reset cause (read SRC_SRSR once at init) ------------------------------
static hal_reset_cause_t s_reset_cause = HAL_RESET_POR;

// --- Alarm pin configuration -----------------------------------------------
static const uint8_t ALARM_PINS[14] = {
     0,  1,  5,  6,  8,  9, 11,   // CH[ 0.. 6]
    12, 14, 15, 16, 17, 22, 23     // CH[ 7..13]
};

static bool s_alarm_active_high[14];   // true = alarm asserted when pin HIGH

// --- Pixel chain (WS2812Serial, pin 29, DMA) ------------------------------
#define PIXEL_MAX 150
static byte         s_draw_mem[PIXEL_MAX * 3];
DMAMEM static byte  s_disp_mem[PIXEL_MAX * 12];
static WS2812Serial s_pixels(PIXEL_MAX, s_disp_mem, s_draw_mem, 29, WS2812_GRB);

#define PIN_BUTTON    18
#define PIN_DIM_IN    26   // A12
#define PIN_BUS_SENSE 27   // A13

static const uint8_t LED_PINS[3] = { 25, 24, 28 };  // R, G, B

void hal_init(void) {
    // Capture reset cause from SRC_SRSR before it is cleared.
    uint32_t srsr = SRC_SRSR;
    if (srsr & (SRC_SRSR_WDOG_RST_B | SRC_SRSR_WDOG3_RST_B))
        s_reset_cause = HAL_RESET_WATCHDOG;
    else if (srsr & SRC_SRSR_IPP_RESET_B)
        s_reset_cause = HAL_RESET_POR;
    else
        s_reset_cause = HAL_RESET_OTHER;

    // Alarm pins: do NOT configure pull here.
    // hal_alarm_configure() is called by app_init() after alarm_cfg_load()
    // with the correct pull per channel.  Pre-set polarity to active-high.
    for (int i = 0; i < 14; i++) {
        s_alarm_active_high[i] = true;
        // Leave as INPUT (no pull) until hal_alarm_configure() is called.
        pinMode(ALARM_PINS[i], INPUT);
    }

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    analogReadResolution(12);
    analogWriteResolution(12);

    for (int i = 0; i < 3; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        analogWrite(LED_PINS[i], 0);  // N-FET: 0 duty = off
    }

    // Lighting buck — keep disabled until config loaded (pin 30 HIGH = FET on = EN low = off)
    pinMode(30, OUTPUT);
    digitalWrite(30, HIGH);
    pinMode(31, INPUT_PULLDOWN);  // PG — open-drain; pull-down holds low when buck is off

    // Pixel chain
    s_pixels.begin();

    Serial.begin(115200);
    audio_glue_init();
}

hal_reset_cause_t hal_reset_cause(void) { return s_reset_cause; }

void hal_alarm_configure(uint8_t ch, bool active_high, hal_pull_t pull) {
    if (ch >= 14) return;
    s_alarm_active_high[ch] = active_high;
    int mode;
    switch (pull) {
    case HAL_PULL_DOWN: mode = INPUT_PULLDOWN; break;
    case HAL_PULL_UP:   mode = INPUT_PULLUP;   break;
    default:            mode = INPUT;           break;
    }
    pinMode(ALARM_PINS[ch], mode);
}

uint32_t hal_millis(void) {
    return millis();
}

bool hal_read_alarm(uint8_t channel) {
    if (channel >= 14) return false;
    bool raw = digitalRead(ALARM_PINS[channel]);
    return s_alarm_active_high[channel] ? raw : !raw;  // apply configured polarity
}

bool hal_read_button(void) {
    return !digitalRead(PIN_BUTTON);  // active-low → positive logic
}

uint16_t hal_adc_read(hal_adc_ch_t ch) {
    switch (ch) {
    case HAL_ADC_DIM_IN:    return (uint16_t)analogRead(PIN_DIM_IN);
    case HAL_ADC_BUS_SENSE: return (uint16_t)analogRead(PIN_BUS_SENSE);
    default:                return 0u;
    }
}

void hal_set_led_duty(hal_led_t led, uint16_t duty) {
    if (led >= 3u) return;  // HAL_LED_AUX not wired; future pixel_lighting
    // N-FET drive: duty maps directly — 0 = off, 4095 = full bright.
    analogWrite(LED_PINS[led], duty);
}

void hal_audio_play(uint8_t wav_id) {
    audio_glue_play(wav_id);
}

bool hal_audio_busy(void)  { return audio_glue_busy();  }
bool hal_audio_sd_ok(void) { return audio_glue_sd_ok(); }

void hal_pixels_write(const uint8_t *rgb_buf, uint16_t n_pixels) {
    if (n_pixels > PIXEL_MAX) n_pixels = PIXEL_MAX;
    for (uint16_t i = 0; i < n_pixels; i++) {
        s_pixels.setPixel(i, rgb_buf[i*3], rgb_buf[i*3+1], rgb_buf[i*3+2]);
    }
    // Blank any pixels beyond n_pixels up to chain length
    for (uint16_t i = n_pixels; i < PIXEL_MAX; i++) s_pixels.setPixel(i, 0, 0, 0);
    s_pixels.show();
}

void hal_lbuck_enable(bool en) {
    // Pin 30 HIGH → N-FET on → EN pulled low → buck off
    // Pin 30 LOW  → N-FET off → EN floats/high → buck runs
    digitalWrite(30, en ? LOW : HIGH);
}

bool hal_lbuck_pg(void) {
    return digitalRead(31) == HIGH;
}

uint8_t hal_eeprom_get(uint16_t addr) {
    return EEPROM.read(addr);
}

void hal_eeprom_put(uint16_t addr, uint8_t val) {
    EEPROM.update(addr, val);   // update only writes if value changed
}

hal_sd_status_t hal_sd_read_file(const char *path, char *buf, size_t max_len, size_t *out_len) {
    if (!SD.exists(path)) return HAL_SD_NOT_FOUND;
    File f = SD.open(path);
    if (!f) return HAL_SD_IO_ERROR;
    size_t file_size = (size_t)f.size();
    if (file_size >= max_len) { f.close(); return HAL_SD_TOO_BIG; }
    size_t n = f.read(buf, max_len - 1);
    f.close();
    if (n != file_size) return HAL_SD_IO_ERROR;
    buf[n] = '\0';
    if (out_len) *out_len = n;
    return HAL_SD_OK;
}

void hal_log_write(const uint8_t *buf, size_t n) {
    Serial.write(buf, n);
}

void hal_watchdog_enable(uint32_t timeout_ms) {
    // WDOG1 timeout = (WT + 1) * 0.5 s.  Clamp to [0.5 s, 128 s].
    uint32_t half_secs = (timeout_ms + 499u) / 500u;
    if (half_secs == 0) half_secs = 1;
    if (half_secs > 256) half_secs = 256;
    uint8_t wt = (uint8_t)(half_secs - 1u);

    // Disable power-down counter (PDE bit) before touching WCR.
    WDOG1_WMCR = 0;
    // Enable watchdog: WDE | SRS (deasserted) | WDA (deasserted) | WT | WDZST.
    // Once WDE is set it cannot be cleared without a reset.
    WDOG1_WCR = WDOG_WCR_WDE | WDOG_WCR_SRS | WDOG_WCR_WDA
              | WDOG_WCR_WT(wt) | WDOG_WCR_WDZST;
}

void hal_watchdog_kick(void) {
    // Service sequence: write 0x5555 then 0xAAAA to WSR within one bus cycle.
    WDOG1_WSR = 0x5555u;
    WDOG1_WSR = 0xAAAAu;
}
