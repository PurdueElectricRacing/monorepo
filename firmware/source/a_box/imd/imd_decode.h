/**
 * @file imd_decode.h
 * @brief ABOX IMD PWM measurement and state reporting
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef IMD_DECODE_H
#define IMD_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include "can_library/generated/can_types.h"
#include "imd.h"

typedef struct {
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t timestamp_ms;
    bool valid;
} imd_capture_t;

typedef struct {
    imd_capture_t latest;
    uint32_t last_edge_ms;
    bool armed;
} imd_capture_tracker_t;

typedef struct {
    imd_state_t state;
    uint16_t r_iso_kohm;
    imd_r_iso_quality_t quality;
} imd_measurement_t;

// Called once per rising edge
// cycle_ok includes falling-edge/overrun checks
void imd_capture_update(
    volatile imd_capture_tracker_t *tracker,
    uint32_t period_ticks,
    uint32_t high_ticks,
    uint32_t now_ms,
    bool cycle_ok
);

// Decodes an IMD capture into a measurement.
imd_measurement_t imd_decode(const imd_capture_t* capture, uint32_t now_ms);

#endif
