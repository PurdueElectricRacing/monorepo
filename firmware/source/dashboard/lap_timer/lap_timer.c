/**
 * @file lap_timer.c
 * @brief DASHBOARD lap timer task implementations
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include "lap_timer.h"
#include "can_library/generated/DASHBOARD.h"

static bool is_timing = false;
static bool is_finish_line_set = false;
static bool is_lap_complete = false;

// every point below is meters east/north of the position the driver pressed at
static geodetic_coord_t origin = {0.0, 0.0};
static vector2_t start_point = {0.0f, 0.0f};
static vector2_t last_point = {0.0f, 0.0f};
static segment2_t finish_line = {0};

static inline vector2_t current_position(void) {
    if (can_data.gps_coordinates.is_stale()) {
        return last_point;
    }
    geodetic_coord_t fix = geodetic_from_scaled(
        can_data.gps_coordinates.latitude,
        can_data.gps_coordinates.longitude
    );

    return geodetic_to_local(origin, fix);
}

void lap_timer_onpress(void) {
    if (can_data.gps_coordinates.is_stale()) {
        return;
    }

    origin = geodetic_from_scaled(
        can_data.gps_coordinates.latitude,
        can_data.gps_coordinates.longitude
    );

    // the press position defines the origin, so it projects onto (0, 0)
    start_point = current_position();
    last_point = start_point;

    is_timing = true;
    is_finish_line_set = false;
    is_lap_complete = false;
}

void lap_timer_periodic(void) {
    if (!is_timing || is_lap_complete || can_data.gps_coordinates.is_stale()) {
        return;
    }

    vector2_t current_point = current_position();

    if (!is_finish_line_set) {
        // wait for the car to clear the capture distance so that the path it
        // took is long enough to give a trustworthy heading, then lay the
        // finish line across that path
        if (vector2_distance(start_point, current_point) >= LAP_TIMER_CAPTURE_DISTANCE_M) {
            segment2_t heading = {.start = start_point, .end = current_point};
            finish_line = segment2_perpendicular_bisector(heading, LAP_TIMER_FINISH_HALF_WIDTH_M);
            is_finish_line_set = true;
        }

        last_point = current_point;
        return;
    }

    segment2_t travelled = {.start = last_point, .end = current_point};
    if (segment2_intersects(travelled, finish_line)) {
        is_lap_complete = true;
        is_timing = false;
        return;
    }

    last_point = current_point;
}
