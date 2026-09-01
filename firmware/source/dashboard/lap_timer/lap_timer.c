/**
 * @file lap_timer.c
 * @brief DASHBOARD lap timer task implementations
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include "lap_timer.h"

#include "can_library/generated/DASHBOARD.h"

static bool lap_timer_active = false;
static bool lap_timer_start_recorded = false;
static bool lap_timer_l1_recorded = false;
static bool lap_timer_complete = false;
static lap_timer_origin_t lap_timer_origin = {0.0, 0.0};
static lap_timer_point_t lap_timer_start_point = {0.0, 0.0};
static lap_timer_point_t lap_timer_l1_end_point = {0.0, 0.0};
static lap_timer_point_t lap_timer_last_point = {0.0, 0.0};

double lap_timer_abs(double value) {
    return value < 0.0 ? -value : value;
}

double lap_timer_max(double a, double b) {
    return a > b ? a : b;
}

double lap_timer_min(double a, double b) {
    return a < b ? a : b;
}

double lap_timer_sqrt(double value) {
    if (value <= 0.0) {
        return 0.0;
    }

    double guess = value;
    if (guess < 1.0) {
        guess = 1.0;
    }

    for (int i = 0; i < 10; ++i) {
        double next = 0.5 * (guess + (value / guess));
        if (lap_timer_abs(next - guess) <= 1e-9) {
            return next;
        }
        guess = next;
    }

    return guess;
}

double lap_timer_cos(double radians) {
    double x2 = radians * radians;
    return 1.0 - (x2 / 2.0) + (x2 * x2 / 24.0) - (x2 * x2 * x2 / 720.0);
}

double lap_timer_hypot(double x, double y) {
    return lap_timer_sqrt((x * x) + (y * y));
}

int lap_timer_orientation(lap_timer_point_t *p, lap_timer_point_t *q, lap_timer_point_t *r) {
    double value = ((q->y - p->y) * (r->x - p->x)) - ((q->x - p->x) * (r->y - p->y));

    if (lap_timer_abs(value) <= LAP_TIMER_EPSILON) {
        return 0;
    }
    return value > 0.0 ? 1 : 2;
}

bool lap_timer_on_segment(lap_timer_point_t *p, lap_timer_point_t *q, lap_timer_point_t *r) {
    return (q->x <= lap_timer_max(p->x, r->x) + LAP_TIMER_EPSILON)
        && (q->x >= lap_timer_min(p->x, r->x) - LAP_TIMER_EPSILON)
        && (q->y <= lap_timer_max(p->y, r->y) + LAP_TIMER_EPSILON)
        && (q->y >= lap_timer_min(p->y, r->y) - LAP_TIMER_EPSILON);
}

bool lap_timer_segments_intersect(
    lap_timer_point_t *p1,
    lap_timer_point_t *q1,
    lap_timer_point_t *p2,
    lap_timer_point_t *q2
) {
    int o1 = lap_timer_orientation(p1, q1, p2);
    int o2 = lap_timer_orientation(p1, q1, q2);
    int o3 = lap_timer_orientation(p2, q2, p1);
    int o4 = lap_timer_orientation(p2, q2, q1);

    if (o1 == 0 && lap_timer_on_segment(p1, p2, q1)) {
        return true;
    }
    if (o2 == 0 && lap_timer_on_segment(p1, q2, q1)) {
        return true;
    }
    if (o3 == 0 && lap_timer_on_segment(p2, p1, q2)) {
        return true;
    }
    if (o4 == 0 && lap_timer_on_segment(p2, q1, q2)) {
        return true;
    }

    return ((o1 != o2) && (o3 != o4));
}

lap_timer_point_t lap_timer_gps_to_local(int32_t latitude, int32_t longitude) {
    double latitude_deg = (double)latitude * 1e-7;
    double longitude_deg = (double)longitude * 1e-7;

    double lat_delta_deg = latitude_deg - lap_timer_origin.latitude_deg;
    double lon_delta_deg = longitude_deg - lap_timer_origin.longitude_deg;
    double origin_lat_rad = lap_timer_origin.latitude_deg * LAP_TIMER_DEG_TO_RAD;
    double cos_lat = lap_timer_cos(origin_lat_rad);

    lap_timer_point_t point = {
        .x = lon_delta_deg * LAP_TIMER_METERS_PER_DEG * cos_lat,
        .y = lat_delta_deg * LAP_TIMER_METERS_PER_DEG,
    };

    return point;
}

bool lap_timer_l2_crossed(lap_timer_point_t *previous_point, lap_timer_point_t *current_point) {
    lap_timer_point_t l1_start = lap_timer_start_point;
    lap_timer_point_t l1_end = lap_timer_l1_end_point;
    lap_timer_point_t midpoint = {
        .x = (l1_start.x + l1_end.x) * 0.5,
        .y = (l1_start.y + l1_end.y) * 0.5,
    };
    lap_timer_point_t line_direction = {
        .x = l1_end.x - l1_start.x,
        .y = l1_end.y - l1_start.y,
    };
    double line_length = lap_timer_hypot(line_direction.x, line_direction.y);

    if (line_length <= LAP_TIMER_EPSILON) {
        return false;
    }

    lap_timer_point_t normal = {
        .x = -line_direction.y / line_length,
        .y = line_direction.x / line_length,
    };
    lap_timer_point_t l2_start = {
        .x = midpoint.x - (normal.x * LAP_TIMER_L2_HALF_LENGTH_M),
        .y = midpoint.y - (normal.y * LAP_TIMER_L2_HALF_LENGTH_M),
    };
    lap_timer_point_t l2_end = {
        .x = midpoint.x + (normal.x * LAP_TIMER_L2_HALF_LENGTH_M),
        .y = midpoint.y + (normal.y * LAP_TIMER_L2_HALF_LENGTH_M),
    };

    return lap_timer_segments_intersect(previous_point, current_point, &l2_start, &l2_end);
}

void lap_timer_onpress(void) {
    if (can_data.gps_coordinates.is_stale()) {
        return;
    }

    lap_timer_active = true;
    lap_timer_start_recorded = false;
    lap_timer_l1_recorded = false;
    lap_timer_complete = false;

    lap_timer_origin.latitude_deg = (double)can_data.gps_coordinates.latitude * 1e-7;
    lap_timer_origin.longitude_deg = (double)can_data.gps_coordinates.longitude * 1e-7;

    lap_timer_start_point = lap_timer_gps_to_local(
        can_data.gps_coordinates.latitude,
        can_data.gps_coordinates.longitude
    );
    lap_timer_last_point = lap_timer_start_point;
    lap_timer_start_recorded = true;
}

void lap_timer_periodic(void) {
    if (!lap_timer_active || lap_timer_complete || can_data.gps_coordinates.is_stale()) {
        return;
    }

    lap_timer_point_t current_point = lap_timer_gps_to_local(
        can_data.gps_coordinates.latitude,
        can_data.gps_coordinates.longitude
    );

    if (!lap_timer_start_recorded) {
        lap_timer_start_point = current_point;
        lap_timer_last_point = current_point;
        lap_timer_start_recorded = true;
        return;
    }

    if (!lap_timer_l1_recorded) {
        double delta = lap_timer_hypot(
            current_point.x - lap_timer_start_point.x,
            current_point.y - lap_timer_start_point.y
        );
        if (delta >= LAP_TIMER_CAPTURE_DISTANCE_M) {
            lap_timer_l1_end_point = current_point;
            lap_timer_l1_recorded = true;
        }
        lap_timer_last_point = current_point;
        return;
    }

    if (lap_timer_l2_crossed(&lap_timer_last_point, &current_point)) {
        lap_timer_complete = true;
        lap_timer_active = false;
        return;
    }

    lap_timer_last_point = current_point;
}