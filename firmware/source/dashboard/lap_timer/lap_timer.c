/**
 * @file lap_timer.c
 * @brief DASHBOARD lap timer task implementations
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include "lap_timer.h"

#include "can_library/faults_common.h"
#include "can_library/generated/DASHBOARD.h"
#include "common/utils/geodetic.h"
#include "common/utils/geometry.h"
#include "common/utils/linear_algebra.h"

static lap_timer_state_t lap_timer_state = LAP_TIMER_STATE_IDLE;
static uint32_t lap_timer_start_time_ms = 0;
static uint32_t lap_timer_elapsed_time_ms = 0;
static geodetic_coord_t lap_timer_origin = {0.0f, 0.0f};
static vector2_t lap_timer_start_point = {0.0f, 0.0f};
static vector2_t lap_timer_l1_end_point = {0.0f, 0.0f};
static vector2_t lap_timer_last_point = {0.0f, 0.0f};

static vector2_t lap_timer_gps_to_local(int32_t latitude, int32_t longitude) {
    const geodetic_coord_t coordinate = geodetic_from_scaled(
        (float)latitude * UNPACK_COEFF_GPS_COORDINATES_LATITUDE,
        (float)longitude * UNPACK_COEFF_GPS_COORDINATES_LONGITUDE
    );

    return geodetic_to_local(lap_timer_origin, coordinate);
}

static bool lap_timer_l2_crossed(const vector2_t previous_point, const vector2_t current_point) {
    const segment2_t heading = {
        .start = lap_timer_start_point,
        .end = lap_timer_l1_end_point,
    };
    const segment2_t finish_line = segment2_perpendicular_bisector(
        heading,
        LAP_TIMER_FINISH_HALF_WIDTH_M
    );
    const segment2_t travelled = {
        .start = previous_point,
        .end = current_point,
    };

    return segment2_intersects(travelled, finish_line);
}

void lap_timer_onpress(void) {
    if (can_data.gps_coordinates.is_stale() ||
        !is_clear(FAULT_ID_GPS_INVALID_FIX) ||
        !is_clear(FAULT_ID_GPS_WEAK_FIX)) {
        return;
    }

    lap_timer_state = LAP_TIMER_STATE_CAPTURING_HEADING;
    lap_timer_start_time_ms = xTaskGetTickCount();
    lap_timer_elapsed_time_ms = 0;

    lap_timer_origin = geodetic_from_scaled(
        (float)can_data.gps_coordinates.latitude * UNPACK_COEFF_GPS_COORDINATES_LATITUDE,
        (float)can_data.gps_coordinates.longitude * UNPACK_COEFF_GPS_COORDINATES_LONGITUDE
    );

    lap_timer_start_point = lap_timer_gps_to_local(
        can_data.gps_coordinates.latitude,
        can_data.gps_coordinates.longitude
    );
    lap_timer_last_point = lap_timer_start_point;
}

void lap_timer_periodic(void) {
    if (can_data.gps_coordinates.is_stale()) {
        return;
    }

    switch (lap_timer_state) {
        case LAP_TIMER_STATE_IDLE:
        case LAP_TIMER_STATE_COMPLETE:
            return;

        case LAP_TIMER_STATE_CAPTURING_HEADING: {
            const vector2_t current_point = lap_timer_gps_to_local(
                can_data.gps_coordinates.latitude,
                can_data.gps_coordinates.longitude
            );
            const float delta = vector2_distance(current_point, lap_timer_start_point);

            if (delta >= LAP_TIMER_CAPTURE_DISTANCE_M) {
                lap_timer_l1_end_point = current_point;
                lap_timer_state = LAP_TIMER_STATE_TIMING;
            }
            lap_timer_last_point = current_point;
            return;
        }

        case LAP_TIMER_STATE_TIMING: {
            const vector2_t current_point = lap_timer_gps_to_local(
                can_data.gps_coordinates.latitude,
                can_data.gps_coordinates.longitude
            );

            if (lap_timer_l2_crossed(lap_timer_last_point, current_point)) {
                lap_timer_elapsed_time_ms = xTaskGetTickCount() - lap_timer_start_time_ms;
                lap_timer_state = LAP_TIMER_STATE_COMPLETE;
                return;
            }

            lap_timer_last_point = current_point;
            return;
        }

        default:
            lap_timer_state = LAP_TIMER_STATE_IDLE;
            return;
    }
}

uint32_t lap_timer_elapsed_ms(void) {
    if (lap_timer_state == LAP_TIMER_STATE_CAPTURING_HEADING ||
        lap_timer_state == LAP_TIMER_STATE_TIMING) {
        return xTaskGetTickCount() - lap_timer_start_time_ms;
    }

    return lap_timer_elapsed_time_ms;
}
