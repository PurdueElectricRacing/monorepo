#include "imd_decode.h"

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

static bool is_between(double value, double minimum, double maximum) {
    return value >= minimum && value <= maximum;
}

static bool frequency_matches(double measured_hz, double expected_hz) {
    double tolerance_hz = expected_hz * IMD_FREQUENCY_TOLERANCE_PCT / IMD_PERCENT_SCALE;
    return is_between(measured_hz, expected_hz - tolerance_hz, expected_hz + tolerance_hz);
}

static imd_state_t state_from_pwm(double frequency_hz, double duty_pct) {
    if (frequency_matches(frequency_hz, IMD_NORMAL_FREQUENCY_HZ)) {
        if (is_between(duty_pct, IMD_MEASUREMENT_MIN_DUTY_PCT, IMD_MEASUREMENT_MAX_DUTY_PCT)) {
            return IMD_STATE_NORMAL;
        }
    } else if (frequency_matches(frequency_hz, IMD_UNDERVOLTAGE_FREQUENCY_HZ)) {
        if (is_between(duty_pct, IMD_MEASUREMENT_MIN_DUTY_PCT, IMD_MEASUREMENT_MAX_DUTY_PCT)) {
            return IMD_STATE_UNDERVOLTAGE;
        }
    } else if (frequency_matches(frequency_hz, IMD_STARTUP_FREQUENCY_HZ)) {
        bool startup_good = is_between(duty_pct, IMD_MEASUREMENT_MIN_DUTY_PCT, IMD_STARTUP_GOOD_MAX_DUTY_PCT);
        bool startup_bad = is_between(duty_pct, IMD_STARTUP_BAD_MIN_DUTY_PCT, IMD_MEASUREMENT_MAX_DUTY_PCT);
        if (startup_good || startup_bad) {
            return IMD_STATE_SPEED_START;
        }
    } else if (frequency_matches(frequency_hz, IMD_DEVICE_ERROR_FREQUENCY_HZ)) {
        if (is_between(duty_pct, IMD_FAULT_MIN_DUTY_PCT, IMD_FAULT_MAX_DUTY_PCT)) {
            return IMD_STATE_DEVICE_ERROR;
        }
    } else if (frequency_matches(frequency_hz, IMD_CONNECTION_FAULT_FREQUENCY_HZ)) {
        if (is_between(duty_pct, IMD_FAULT_MIN_DUTY_PCT, IMD_FAULT_MAX_DUTY_PCT)) {
            return IMD_STATE_CONNECTION_FAULT;
        }
    }
    return IMD_STATE_UNKNOWN;
}

static void calculate_resistance(double duty_pct, imd_measurement_t *result) {
    // At 5% the IMD indicates infinite resistance. Avoid divide by zero.
    if (duty_pct <= IMD_MEASUREMENT_MIN_DUTY_PCT) {
        result->quality = IMD_R_ISO_QUALITY_ABOVE_RANGE;
        return;
    }

    // Datasheet: R_iso = (90% * 1200 kOhm) / (duty - 5%) - 1200 kOhm.
    double duty_span_pct = IMD_MEASUREMENT_MAX_DUTY_PCT - IMD_MEASUREMENT_MIN_DUTY_PCT;
    double resistance_kohm = duty_span_pct * IMD_RESISTANCE_AT_HALF_DUTY_KOHM /
                            (duty_pct - IMD_MEASUREMENT_MIN_DUTY_PCT) -
                            IMD_RESISTANCE_AT_HALF_DUTY_KOHM;
    if (resistance_kohm > IMD_MAX_MEASURABLE_RESISTANCE_KOHM) {
        result->quality = IMD_R_ISO_QUALITY_ABOVE_RANGE;
        return;
    }

    result->r_iso_kohm = (uint16_t)(resistance_kohm + 0.5); // Round to the nearest kOhm.
    result->quality = IMD_R_ISO_QUALITY_VALID;
}

imd_measurement_t imd_decode(const imd_capture_t *capture, uint32_t now_ms) {
    imd_measurement_t result = {
        .state = IMD_STATE_UNKNOWN,
        .r_iso_kohm = 0U,
        .quality = IMD_R_ISO_QUALITY_INVALID,
    };

    uint32_t capture_age_ms = (uint32_t)(now_ms - capture->timestamp_ms);
    bool capture_stale = capture_age_ms > IMD_CAPTURE_TIMEOUT_MS;
    if (!capture->valid || capture_stale) {
        return result;
    }
    if (capture->period_ticks == 0U || capture->high_ticks > capture->period_ticks) {
        return result;
    }

    // The non-inverting MLS interface encodes duty in the GPIO low time
    uint32_t low_ticks = capture->period_ticks - capture->high_ticks;
    double frequency_hz = (double)IMD_TIMER_TICK_HZ / capture->period_ticks;
    double duty_pct = IMD_PERCENT_SCALE * low_ticks / capture->period_ticks;
    result.state = state_from_pwm(frequency_hz, duty_pct);

    bool resistance_available = result.state == IMD_STATE_NORMAL ||
                                result.state == IMD_STATE_UNDERVOLTAGE;
    if (resistance_available) {
        calculate_resistance(duty_pct, &result);
    }
    return result;
}
