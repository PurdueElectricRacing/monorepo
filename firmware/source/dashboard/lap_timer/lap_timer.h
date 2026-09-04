#ifndef LAP_TIMER_H
#define LAP_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "common/utils/geodetic.h"
#include "common/utils/geometry.h"
#include "common/utils/linear_algebra.h"

/**
 * @file lap_timer.h
 * @brief DASHBOARD lap timer task implementations
 * 
 * @author Amruth Nadimpally(nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

// how far the car must travel past the start point to establish its heading
static constexpr float LAP_TIMER_CAPTURE_DISTANCE_M = 1.0f;

// how far the finish line extends either side of the path the car took
static constexpr float LAP_TIMER_FINISH_HALF_WIDTH_M = 1.0f;

static constexpr uint32_t LAP_TIMER_PERIOD_MS = 200;

void lap_timer_onpress(void);
void lap_timer_periodic(void);

#endif // LAP_TIMER_H
