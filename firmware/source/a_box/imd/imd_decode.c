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
