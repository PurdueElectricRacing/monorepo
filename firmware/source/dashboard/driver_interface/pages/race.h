#ifndef RACE_H
#define RACE_H

/**
 * @file race.h
 * @brief Race page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

 #define RACE_STRING "race"

// Nextion object names
#define THROT_BAR    "throt"
#define RGN_BAR      "regen"
#define BRK_BAR      "brake"
#define FL_BAR       "FL"
#define FR_BAR       "FR"
#define RL_BAR       "RL"
#define RR_BAR       "RR"
#define PUMP_CHECK   "pump"
#define BATT_TEMP    "BTemp"
#define BATT_VOLT    "volt"
#define BATT_CURR    "amp"
#define MOTOR_TEMP   "MTemp"
#define IGBT_TEMP    "IGBTemp"
#define CAR_STATE    "stat"
#define SPEED        "speed"
#define RACE_TV_ON   "tv"
#define CURRENT_TIME "current"
#define BEST_TIME    "best"
#define LAP_TIME     "lap"

typedef struct {
    uint32_t minutes;
    uint32_t seconds;
    uint32_t centiseconds;
} time_display_t;

void race_telemetry_update();

#endif // RACE_H