#ifndef LAP_TIMER_H
#define LAP_TIMER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file lap_timer.h
 * @brief DASHBOARD lap timer task implementations
 * 
 * @author Amruth Nadimpally(nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#define LAP_TIMER_DEG_TO_RAD 0.017453292519943295
#define LAP_TIMER_METERS_PER_DEG 111132.0
#define LAP_TIMER_CAPTURE_DISTANCE_M 1.0
#define LAP_TIMER_L2_HALF_LENGTH_M 1.0
#define LAP_TIMER_EPSILON 1e-6

typedef struct {
    double x;
    double y;
} lap_timer_point_t;

typedef struct {
    double latitude_deg;
    double longitude_deg;
} lap_timer_origin_t;

static constexpr uint32_t LAP_TIMER_PERIOD_MS = 200;

void lap_timer_onpress(void);
void lap_timer_periodic(void);

#endif // LAP_TIMER_H