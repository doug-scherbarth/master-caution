// lib/app/dimmer_gesture.c

#include "dimmer_gesture.h"

typedef enum {
    GS_IDLE,
    GS_DIP_ARMED,   // ratio went below low; waiting for it to return
    GS_BUMP_ARMED,  // ratio went above high; waiting for it to return
} gesture_state_t;

static gesture_state_t s_state;
static uint32_t        s_arm_time;
static uint8_t         s_low;        // threshold in 0-255 ratio units
static uint8_t         s_high;
static uint32_t        s_timeout_ms;

void dimmer_gesture_init(uint8_t low_pct, uint8_t high_pct, uint32_t timeout_ms) {
    s_state      = GS_IDLE;
    s_arm_time   = 0;
    s_low        = (uint8_t)((uint32_t)low_pct  * 255u / 100u);
    s_high       = (uint8_t)((uint32_t)high_pct * 255u / 100u);
    s_timeout_ms = timeout_ms;
}

bool dimmer_gesture_tick(uint32_t now_ms, uint8_t ratio) {
    switch (s_state) {
        case GS_IDLE:
            if (ratio < s_low) {
                s_state    = GS_DIP_ARMED;
                s_arm_time = now_ms;
            } else if (ratio > s_high) {
                s_state    = GS_BUMP_ARMED;
                s_arm_time = now_ms;
            }
            break;

        case GS_DIP_ARMED:
            if ((now_ms - s_arm_time) > s_timeout_ms) {
                s_state = GS_IDLE;
            } else if (ratio >= s_low) {
                s_state = GS_IDLE;
                return true;
            }
            break;

        case GS_BUMP_ARMED:
            if ((now_ms - s_arm_time) > s_timeout_ms) {
                s_state = GS_IDLE;
            } else if (ratio <= s_high) {
                s_state = GS_IDLE;
                return true;
            }
            break;
    }
    return false;
}
