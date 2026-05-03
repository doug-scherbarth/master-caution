// lib/app/alarm_table.c
#include "alarm_table.h"

// --- Composite alarm assertion functions ---------------------------------
//
// Each takes the post-debounce channel-state array (true = asserted) and
// returns whether the composite alarm should be considered active.

static bool flaps_overspeed(const bool *ch) {
    // Spec §2.3: assert only when flaps deployed AND airspeed above Vfe.
    return ch[CH_FLAPS_DEPLOYED] && ch[CH_AIRSPEED_OVER_VFE];
}

// --- Alarm table ---------------------------------------------------------

const alarm_descriptor_t ALARM_TABLE[] = {
  { "CO_DETECT",       SEV_HIGH, SRC_DIRECT,    CH_CO_DETECT,       NULL,            WAV_CO         },
  { "L_MAG_FAIL",      SEV_HIGH, SRC_DIRECT,    CH_L_MAG_FAIL,      NULL,            WAV_L_MAG      },
  { "R_MAG_FAIL",      SEV_HIGH, SRC_DIRECT,    CH_R_MAG_FAIL,      NULL,            WAV_R_MAG      },
  { "OIL_PRESS_LOW",   SEV_HIGH, SRC_DIRECT,    CH_OIL_PRESS_LOW,   NULL,            WAV_OIL_PRESS  },
  { "CHT_OVERTEMP",    SEV_HIGH, SRC_DIRECT,    CH_CHT_OVERTEMP,    NULL,            WAV_CHT        },
  { "EGT_OVERTEMP",    SEV_HIGH, SRC_DIRECT,    CH_EGT_OVERTEMP,    NULL,            WAV_EGT        },
  { "PRIM_ALT_FAIL",   SEV_MED,  SRC_DIRECT,    CH_PRIM_ALT_FAIL,   NULL,            WAV_PRIM_ALT   },
  { "SEC_ALT_FAIL",    SEV_MED,  SRC_DIRECT,    CH_SEC_ALT_FAIL,    NULL,            WAV_SEC_ALT    },
  { "PITOT_HEAT_FAIL", SEV_MED,  SRC_DIRECT,    CH_PITOT_HEAT_FAIL, NULL,            WAV_PITOT      },
  { "OIL_TEMP_HIGH",   SEV_MED,  SRC_DIRECT,    CH_OIL_TEMP_HIGH,   NULL,            WAV_OIL_TEMP   },
  { "FUEL_PRESS_LOW",  SEV_MED,  SRC_DIRECT,    CH_FUEL_PRESS_LOW,  NULL,            WAV_FUEL_PRESS },
  { "FLAPS_OVERSPEED", SEV_LOW,  SRC_COMPOSITE, 0,                  flaps_overspeed, WAV_FLAPS_OS   },
  { "BOOST_PUMP_ON",   SEV_LOW,  SRC_DIRECT,    CH_BOOST_PUMP_ON,   NULL,            WAV_BOOST      },
};

const uint8_t ALARM_COUNT = sizeof(ALARM_TABLE) / sizeof(ALARM_TABLE[0]);
