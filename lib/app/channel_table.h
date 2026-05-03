// lib/app/channel_table.h
//
// 15 physical input channels on DB-15 #1.
// Indices match the connector pin number minus 1 (ch 0 = pin 1).
// All channels are active-low at the connector; HAL inverts.

#ifndef CHANNEL_TABLE_H
#define CHANNEL_TABLE_H

#include "types.h"

enum channel_id {
    CH_CO_DETECT          = 0,
    CH_L_MAG_FAIL         = 1,
    CH_R_MAG_FAIL         = 2,
    CH_OIL_PRESS_LOW      = 3,
    CH_CHT_OVERTEMP       = 4,
    CH_EGT_OVERTEMP       = 5,
    CH_PRIM_ALT_FAIL      = 6,
    CH_SEC_ALT_FAIL       = 7,
    CH_PITOT_HEAT_FAIL    = 8,
    CH_OIL_TEMP_HIGH      = 9,
    CH_FUEL_PRESS_LOW     = 10,
    CH_FLAPS_DEPLOYED     = 11,
    CH_AIRSPEED_OVER_VFE  = 12,
    CH_BOOST_PUMP_ON      = 13,
    CH_SPARE              = 14,
};

typedef struct {
    const char *name;          // for log/debug
    uint16_t    debounce_ms;   // qualification time per spec §6.2
} channel_descriptor_t;

extern const channel_descriptor_t CHANNEL_TABLE[CHANNEL_COUNT];

#endif // CHANNEL_TABLE_H
