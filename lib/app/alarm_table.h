// lib/app/alarm_table.h
//
// Pilot-facing alarms — 13 in v1.3, sized to ALARM_COUNT_MAX for growth.
// A descriptor declares: name, severity, source kind, source binding, wav id.
// Composite alarms supply a function pointer that operates on the debounced
// channel-state array; direct alarms supply a channel index. No other special
// cases — adding a new alarm is one row in alarm_table.c.

#ifndef ALARM_TABLE_H
#define ALARM_TABLE_H

#include "types.h"
#include "channel_table.h"

typedef enum {
    SRC_DIRECT,     // 1:1 mapping, .channel field is valid
    SRC_COMPOSITE,  // .composite() function pointer evaluates the assertion
} source_kind_t;

typedef bool (*composite_fn_t)(const bool *channel_state);

typedef struct {
    const char     *name;
    severity_t      severity;
    source_kind_t   kind;
    uint8_t         channel;     // SRC_DIRECT only; ignored for composites
    composite_fn_t  composite;   // SRC_COMPOSITE only; NULL for direct
    uint8_t         wav_id;
} alarm_descriptor_t;

// Stable indices into the WAV file table on the SD card.
// Filenames are mapped in audio_queue / hal_teensy.
enum wav_id {
    WAV_CO = 0,
    WAV_L_MAG,
    WAV_R_MAG,
    WAV_OIL_PRESS,
    WAV_CHT,
    WAV_EGT,
    WAV_PRIM_ALT,
    WAV_SEC_ALT,
    WAV_PITOT,
    WAV_OIL_TEMP,
    WAV_FUEL_PRESS,
    WAV_FLAPS_OS,
    WAV_BOOST,
    WAV_COUNT
};

extern const alarm_descriptor_t ALARM_TABLE[];
extern const uint8_t            ALARM_COUNT;

#endif // ALARM_TABLE_H
