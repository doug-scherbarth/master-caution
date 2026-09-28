// lib/app/channel_table.c
#include "channel_table.h"

const channel_descriptor_t CHANNEL_TABLE[CHANNEL_COUNT] = {
    [CH_CO_DETECT]         = { "CO_DETECT",          500, false },
    [CH_L_MAG_FAIL]        = { "L_MAG_FAIL",         500, false },
    [CH_R_MAG_FAIL]        = { "R_MAG_FAIL",         500, false },
    [CH_OIL_PRESS_LOW]     = { "OIL_PRESS_LOW",      500, true  },
    [CH_CHT_OVERTEMP]      = { "CHT_OVERTEMP",       500, false },
    [CH_EGT_OVERTEMP]      = { "EGT_OVERTEMP",       500, false },
    [CH_PRIM_ALT_FAIL]     = { "PRIM_ALT_FAIL",      500, true  },
    [CH_SEC_ALT_FAIL]      = { "SEC_ALT_FAIL",       500, true  },
    [CH_PITOT_HEAT_FAIL]   = { "PITOT_HEAT_FAIL",    500, false },
    [CH_OIL_TEMP_HIGH]     = { "OIL_TEMP_HIGH",      500, false },
    [CH_FUEL_PRESS_LOW]    = { "FUEL_PRESS_LOW",      500, true  },
    [CH_FLAPS_DEPLOYED]    = { "FLAPS_DEPLOYED",      250, false },  // mech switch — faster
    [CH_AIRSPEED_OVER_VFE] = { "AIRSPEED_OVER_VFE",  500, false },
    [CH_BOOST_PUMP_ON]     = { "BOOST_PUMP_ON",       500, false },
};
