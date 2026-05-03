// lib/app/types.h
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Severity ordering matters: higher numeric value wins in max() comparisons.
typedef enum {
    SEV_NONE = 0,   // sentinel: no active alarm
    SEV_LOW  = 1,
    SEV_MED  = 2,
    SEV_HIGH = 3,
} severity_t;

#define CHANNEL_COUNT     15
#define ALARM_COUNT_MAX   16    // sized to allow growth past the 13 baseline

#endif // TYPES_H
