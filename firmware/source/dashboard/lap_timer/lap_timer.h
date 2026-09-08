/**
 * @file lap_timer.h
 * @brief DASHBOARD lap timer task implementations
 * 
 * @author Amruth Nadimpally(nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#ifndef LAP_TIMER_H
#define LAP_TIMER_H

#include <stdint.h>
#include "common/utils/geodetic.h"
#include "common/utils/linear_algebra.h"

// how far the car must travel past the start point to establish its heading
static constexpr float LAP_TIMER_CAPTURE_DISTANCE_M = 1.0f;

// how far the finish line extends either side of the path the car took
static constexpr float LAP_TIMER_FINISH_HALF_WIDTH_M = 1.0f;

typedef enum {
    LAP_TIMER_STATE_IDLE,
    LAP_TIMER_STATE_CAPTURING_HEADING,
    LAP_TIMER_STATE_TIMING,
    LAP_TIMER_STATE_COMPLETE,
} lap_timer_state_t;

typedef struct {
    lap_timer_state_t state;
    uint32_t start_time_ms;
    uint32_t elapsed_time_ms;
    geodetic_coord_t origin;
    vector2_t start_point;
    vector2_t l1_end_point;
    vector2_t last_point;
} lap_timer_context_t;

void lap_timer_onpress(void);
void lap_timer_periodic(void);
uint32_t lap_timer_elapsed_ms(void);

#endif // LAP_TIMER_H
