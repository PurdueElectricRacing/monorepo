/**
 * @file current_sense.h
 * @brief current sensing
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef CURRENT_SENSE_H
#define CURRENT_SENSE_H

#include <stdint.h>
#include "switch.h"

// vTaskDelay(N) only guarantees N - 1 full ticks (ms) of elapsed time
static constexpr uint32_t MUX_SETTLE_DELAY_MS = 2;
static constexpr float MUX_SETTLE_TIME_US = 24.0f;
static_assert(
    MUX_SETTLE_DELAY_MS >= 1 && (MUX_SETTLE_DELAY_MS - 1) > (MUX_SETTLE_TIME_US / 1000.0f), // todo use dearunits here
    "must delay longer than settle time"
);

static constexpr uint32_t CURRENT_SENSE_TASK_PERIOD_MS = 4;
static_assert(
    CURRENT_SENSE_TASK_PERIOD_MS > MUX_SETTLE_DELAY_MS,
    "task period must be longer than mux settle time"
);
void current_sense_periodic(void);

#endif // CURRENT_SENSE_H