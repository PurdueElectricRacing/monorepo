#ifndef LAP_TIMER_H
#define LAP_TIMER_H

/**
 * @file lap_timer.h
 * @brief DASHBOARD lap timer task implementations
 * 
 * @author Amruth Nadimpally(nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include <stdint.h>

static constexpr uint32_t LAP_TIMER_PERIOD_MS = 200;

void lap_timer_onpress(void);
void lap_timer_periodic(void);

#endif // LAP_TIMER_H