// lib/app/dimmer_gesture.c

#include "dimmer_gesture.h"

typedef enum {
    GS_IDLE,
    GS_DIP_ARMED,   // ratio went below LOW; waiting for it to return above LOW
    GS_BUMP_ARMED,  // ratio went above HIGH; waiting for it to return below HIGH
} gesture_state_t;

static gesture_state_t s_state;
static uint32_t        s_arm_time;

void dimmer_gesture_init(void) {
    s_state    = GS_IDLE;
    s_arm_time = 0;
}

bool dimmer_gesture_tick(uint32_t now_ms, uint8_t ratio) {
    switch (s_state) {
        case GS_IDLE:
            if (ratio < DIM_GESTURE_LOW_THRESH) {
                s_state    = GS_DIP_ARMED;
                s_arm_time = now_ms;
            } else if (ratio > DIM_GESTURE_HIGH_THRESH) {
                s_state    = GS_BUMP_ARMED;
                s_arm_time = now_ms;
            }
            break;

        case GS_DIP_ARMED:
            if ((now_ms - s_arm_time) > DIM_GESTURE_TIMEOUT_MS) {
                // dwell too long — treat as intentional dim setting
                s_state = GS_IDLE;
            } else if (ratio >= DIM_GESTURE_LOW_THRESH) {
                // returned above low threshold: gesture complete
                s_state = GS_IDLE;
                return true;
            }
            break;

        case GS_BUMP_ARMED:
            if ((now_ms - s_arm_time) > DIM_GESTURE_TIMEOUT_MS) {
                s_state = GS_IDLE;
            } else if (ratio <= DIM_GESTURE_HIGH_THRESH) {
                s_state = GS_IDLE;
                return true;
            }
            break;
    }
    return false;
}
