// lib/app/dimmer_gesture.c

#include "dimmer_gesture.h"

// 5 percentage points in 0-255 space, rounded: (5*255 + 50) / 100 = 13
// Plain truncation gives 12 and fails the hysteresis boundary test.
#define HYST_COUNTS ((uint8_t)((5u * 255u + 50u) / 100u))

typedef enum {
    GS_SEED,         // waiting for first sample — never arms
    GS_NEUTRAL,      // in the neutral band [low+HYST .. high-HYST]
    GS_LOW,          // below low threshold, not armed (seeded or timed out)
    GS_HIGH,         // above high threshold, not armed
    GS_DIP_ARMED,    // NEUTRAL→LOW transition; waiting for return to low+HYST
    GS_BUMP_ARMED,   // NEUTRAL→HIGH transition; waiting for return to high-HYST
} gesture_state_t;

static gesture_state_t s_state;
static uint32_t        s_arm_time;
static uint8_t         s_low;        // entry threshold: ratio < s_low arms dip
static uint8_t         s_high;       // entry threshold: ratio > s_high arms bump
static uint8_t         s_low_hyst;   // return threshold for dip:  low + HYST
static uint8_t         s_high_hyst;  // return threshold for bump: high - HYST
static uint32_t        s_timeout_ms;

void dimmer_gesture_init(uint8_t low_pct, uint8_t high_pct, uint32_t timeout_ms) {
    s_low       = (uint8_t)((uint32_t)low_pct  * 255u / 100u);
    s_high      = (uint8_t)((uint32_t)high_pct * 255u / 100u);
    s_low_hyst  = s_low  + HYST_COUNTS;
    s_high_hyst = s_high - HYST_COUNTS;
    s_timeout_ms = timeout_ms;
    s_state      = GS_SEED;
    s_arm_time   = 0;
}

bool dimmer_gesture_tick(uint32_t now_ms, uint8_t ratio) {
    switch (s_state) {

    case GS_SEED:
        // First sample: establish which zone we start in, never arm.
        if      (ratio < s_low)  s_state = GS_LOW;
        else if (ratio > s_high) s_state = GS_HIGH;
        else                     s_state = GS_NEUTRAL;
        break;

    case GS_NEUTRAL:
        if      (ratio < s_low)  { s_state = GS_DIP_ARMED;  s_arm_time = now_ms; }
        else if (ratio > s_high) { s_state = GS_BUMP_ARMED; s_arm_time = now_ms; }
        break;

    case GS_LOW:
        // Post-cancel or boot-low: wait until ratio rises past low+HYST before re-arming.
        if (ratio >= s_low_hyst) s_state = GS_NEUTRAL;
        break;

    case GS_HIGH:
        if (ratio <= s_high_hyst) s_state = GS_NEUTRAL;
        break;

    case GS_DIP_ARMED:
        if ((uint32_t)(now_ms - s_arm_time) > s_timeout_ms) {
            s_state = GS_LOW;   // cancelled; must exit to NEUTRAL before re-arming
        } else if (ratio >= s_low_hyst) {
            s_state = GS_NEUTRAL;
            return true;
        }
        break;

    case GS_BUMP_ARMED:
        if ((uint32_t)(now_ms - s_arm_time) > s_timeout_ms) {
            s_state = GS_HIGH;
        } else if (ratio <= s_high_hyst) {
            s_state = GS_NEUTRAL;
            return true;
        }
        break;

    default:
        s_state = GS_NEUTRAL;
        break;
    }
    return false;
}
