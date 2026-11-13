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

static constexpr uint32_t MUX_SETTLE_DELAY_MS = 2;
static constexpr float MUX_SETTLE_TIME = 24E-6f; // 24us
static_assert(
    MUX_SETTLE_DELAY_MS > (MUX_SETTLE_TIME * 1000.0f), // todo use dearunits here
    "must delay longer than settle time"
);

static constexpr uint32_t CURRENT_SENSE_TASK_PERIOD_MS = 4;
static_assert(
    CURRENT_SENSE_TASK_PERIOD_MS > MUX_SETTLE_DELAY_MS,
    "task period must be longer than mux settle time"
);
void current_sense_periodic(void);

#endif // CURRENT_SENSE_H