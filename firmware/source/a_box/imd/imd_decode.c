#include "imd_decode.h"

#include "common/rtos/rtos.h"


void imd_capture_update(
    volatile imd_capture_tracker_t *tracker,
    uint32_t period_ticks,
    uint32_t high_ticks,
    uint32_t now_ms,
    bool cycle_ok
) {
    uint32_t capture_age_ms = (uint32_t)(now_ms - tracker->last_edge_ms);
    bool capture_stale = (capture_age_ms > IMD_CAPTURE_TIMEOUT_MS);
    
    bool continuous = tracker->armed && !capture_stale;
    bool valid = continuous && cycle_ok && (period_ticks != 0U);

    tracker->latest = (imd_capture_t){
        .period_ticks = period_ticks,
        .high_ticks   = high_ticks,
        .timestamp_ms = now_ms,
        .valid        = valid
    };
    tracker->last_edge_ms = now_ms;

    // A clean edge establishes the next cycle's origin
    // After a corrupt acquisition, require another clean origin
    tracker->armed = cycle_ok && (period_ticks != 0U);
}

// Returns true if the period is within +/- 5% of the nominal frequency.
static bool frequency_near(uint32_t period, uint32_t nominal_hz) {
    uint64_t scaled_clock = (uint64_t)IMD_TIMER_TICK_HZ * 100U;
    uint64_t scaled_period = (uint64_t)period * nominal_hz;
    uint64_t lower_bound = scaled_period * 95U;
    uint64_t upper_bound = scaled_period * 105U;
    bool above_lower_bound = scaled_clock >= lower_bound;
    bool below_upper_bound = scaled_clock <= upper_bound;
    return above_lower_bound && below_upper_bound;
}

// Bounds are expressed in tenths of a percent
static bool duty_between(uint32_t active, uint32_t period,
                         uint32_t minimum, uint32_t maximum) {
    uint64_t scaled_active = (uint64_t)active * 1000U;
    uint64_t scaled_period_minimum = (uint64_t)period * minimum;
    uint64_t scaled_period_maximum = (uint64_t)period * maximum;
    bool above_minimum = scaled_active >= scaled_period_minimum;
    bool below_maximum = scaled_active <= scaled_period_maximum;
    return above_minimum && below_maximum;
}
