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

#define LAP_TIMER_DEG_TO_RAD 0.017453292519943295f
#define LAP_TIMER_METERS_PER_DEG 111132.0f
#define LAP_TIMER_L2_HALF_LENGTH_M 1.0f
#define LAP_TIMER_EPSILON 1e-6f

// how far the car must travel past the start point to establish its heading
static constexpr float LAP_TIMER_CAPTURE_DISTANCE_M = 1.0f;

// how far the finish line extends either side of the path the car took
static constexpr float LAP_TIMER_FINISH_HALF_WIDTH_M = 1.0f;

static constexpr uint32_t LAP_TIMER_PERIOD_MS = 200;
typedef enum {
    LAP_TIMER_STATE_IDLE,
    LAP_TIMER_STATE_CAPTURING_HEADING,
    LAP_TIMER_STATE_TIMING,
    LAP_TIMER_STATE_COMPLETE,
} lap_timer_state_t;

void lap_timer_onpress(void);
void lap_timer_periodic(void);
uint32_t lap_timer_elapsed_ms(void);

#endif // LAP_TIMER_H
