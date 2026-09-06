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

typedef struct {
    float x;
    float y;
} lap_timer_point_t;

typedef struct {
    float latitude_deg;
    float longitude_deg;
} lap_timer_origin_t;

static bool lap_timer_active = false;
static bool lap_timer_start_recorded = false;
static bool lap_timer_l1_recorded = false;
static bool lap_timer_complete = false;
static lap_timer_origin_t lap_timer_origin = {0.0f, 0.0f};
static lap_timer_point_t lap_timer_start_point = {0.0f, 0.0f};
static lap_timer_point_t lap_timer_l1_end_point = {0.0f, 0.0f};
static lap_timer_point_t lap_timer_last_point = {0.0f, 0.0f};

static float lap_timer_abs(float value) {
    return value < 0.0f ? -value : value;
}

static float lap_timer_max(float a, float b) {
    return a > b ? a : b;
}

static float lap_timer_min(float a, float b) {
    return a < b ? a : b;
}

static float lap_timer_sqrt(float value) {
    if (value <= 0.0f) {
        return 0.0f;
    }

    float guess = value;
    if (guess < 1.0f) {
        guess = 1.0f;
    }

    for (int i = 0; i < 10; ++i) {
        const float next = 0.5f * (guess + (value / guess));
        if (lap_timer_abs(next - guess) <= 1e-9f) {
            return next;
        }
        guess = next;
    }

    return guess;
}

static float lap_timer_cos(float radians) {
    const float x2 = radians * radians;
    return 1.0f - (x2 / 2.0f) + (x2 * x2 / 24.0f) - (x2 * x2 * x2 / 720.0f);
}

static float lap_timer_hypot(float x, float y) {
    return lap_timer_sqrt((x * x) + (y * y));
}

static int lap_timer_orientation(const lap_timer_point_t *p, const lap_timer_point_t *q, const lap_timer_point_t *r) {
    const float value = ((q->y - p->y) * (r->x - p->x)) - ((q->x - p->x) * (r->y - p->y));

    if (lap_timer_abs(value) <= LAP_TIMER_EPSILON) {
        return 0;
    }
    return value > 0.0f ? 1 : 2;
}

static bool lap_timer_on_segment(const lap_timer_point_t *p, const lap_timer_point_t *q, const lap_timer_point_t *r) {
    return (q->x <= lap_timer_max(p->x, r->x) + LAP_TIMER_EPSILON)
        && (q->x >= lap_timer_min(p->x, r->x) - LAP_TIMER_EPSILON)
        && (q->y <= lap_timer_max(p->y, r->y) + LAP_TIMER_EPSILON)
        && (q->y >= lap_timer_min(p->y, r->y) - LAP_TIMER_EPSILON);
}

static bool lap_timer_segments_intersect(
    const lap_timer_point_t *p1,
    const lap_timer_point_t *q1,
    const lap_timer_point_t *p2,
    const lap_timer_point_t *q2
) {
    const int o1 = lap_timer_orientation(p1, q1, p2);
    const int o2 = lap_timer_orientation(p1, q1, q2);
    const int o3 = lap_timer_orientation(p2, q2, p1);
    const int o4 = lap_timer_orientation(p2, q2, q1);

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

static lap_timer_point_t lap_timer_gps_to_local(int32_t latitude, int32_t longitude) {
    const float latitude_deg = (float)latitude * 1e-7f;
    const float longitude_deg = (float)longitude * 1e-7f;

    const float lat_delta_deg = latitude_deg - lap_timer_origin.latitude_deg;
    const float lon_delta_deg = longitude_deg - lap_timer_origin.longitude_deg;
    const float origin_lat_rad = lap_timer_origin.latitude_deg * LAP_TIMER_DEG_TO_RAD;
    const float cos_lat = lap_timer_cos(origin_lat_rad);

    lap_timer_point_t point = {
        .x = lon_delta_deg * LAP_TIMER_METERS_PER_DEG * cos_lat,
        .y = lat_delta_deg * LAP_TIMER_METERS_PER_DEG,
    };

    return point;
}

static bool lap_timer_l2_crossed(const lap_timer_point_t *previous_point, const lap_timer_point_t *current_point) {
    const lap_timer_point_t l1_start = lap_timer_start_point;
    const lap_timer_point_t l1_end = lap_timer_l1_end_point;
    const lap_timer_point_t midpoint = {
        .x = (l1_start.x + l1_end.x) * 0.5f,
        .y = (l1_start.y + l1_end.y) * 0.5f,
    };
    const lap_timer_point_t line_direction = {
        .x = l1_end.x - l1_start.x,
        .y = l1_end.y - l1_start.y,
    };
    const float line_length = lap_timer_hypot(line_direction.x, line_direction.y);

    if (line_length <= LAP_TIMER_EPSILON) {
        return false;
    }

    const lap_timer_point_t normal = {
        .x = -line_direction.y / line_length,
        .y = line_direction.x / line_length,
    };
    const lap_timer_point_t l2_start = {
        .x = midpoint.x - (normal.x * LAP_TIMER_L2_HALF_LENGTH_M),
        .y = midpoint.y - (normal.y * LAP_TIMER_L2_HALF_LENGTH_M),
    };
    const lap_timer_point_t l2_end = {
        .x = midpoint.x + (normal.x * LAP_TIMER_L2_HALF_LENGTH_M),
        .y = midpoint.y + (normal.y * LAP_TIMER_L2_HALF_LENGTH_M),
    };

    return lap_timer_segments_intersect(previous_point, current_point, &l2_start, &l2_end);
}

void lap_timer_onpress(void) {
    if (can_data.gps_coordinates.is_stale() ||
        !is_clear(FAULT_ID_GPS_INVALID_FIX) ||
        !is_clear(FAULT_ID_GPS_WEAK_FIX)) {
        return;
    }

    lap_timer_active = true;
    lap_timer_start_recorded = false;
    lap_timer_l1_recorded = false;
    lap_timer_complete = false;

    lap_timer_origin.latitude_deg = (float)can_data.gps_coordinates.latitude * 1e-7f;
    lap_timer_origin.longitude_deg = (float)can_data.gps_coordinates.longitude * 1e-7f;

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

    const lap_timer_point_t current_point = lap_timer_gps_to_local(
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
        const float delta = lap_timer_hypot(
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
