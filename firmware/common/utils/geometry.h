#ifndef GEOMETRY_H
#define GEOMETRY_H

/**
 * @file geometry.h
 * @brief Planar computational geometry utility header.
 *
 * Winding and segment intersection tests built on vector2_t. All points must
 * live in the same cartesian frame; units are arbitrary but must be consistent.
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include <stdbool.h>

#include "common/utils/abs.h"
#include "common/utils/linear_algebra.h"
#include "common/utils/max.h"
#include "common/utils/min.h"

// ! cross products scale with segment length, so this tolerance is an area,
// ! not a distance. Sized for segments on the order of meters.
static constexpr float GEOMETRY_EPSILON = 1e-6f;

typedef struct {
    vector2_t start;
    vector2_t end;
} segment2_t;

typedef enum {
    WINDING_COLLINEAR = 0,
    WINDING_CLOCKWISE,
    WINDING_COUNTERCLOCKWISE,
} winding_t;

/**
 * @brief Determine which way the path a -> b -> c turns.
 *
 * @param a The first point
 * @param b The second point
 * @param c The third point
 * @return The winding direction of the three points.
 */
static inline winding_t vector2_winding(const vector2_t a, const vector2_t b, const vector2_t c) {
    float cross = vector2_cross(vector2_sub(b, a), vector2_sub(c, a));

    if (ABS(cross) <= GEOMETRY_EPSILON) {
        return WINDING_COLLINEAR;
    }
    return (cross > 0.0f) ? WINDING_COUNTERCLOCKWISE : WINDING_CLOCKWISE;
}

/**
 * @brief Calculate the length of a segment.
 *
 * @param segment The input segment
 * @return The distance between the segment endpoints.
 */
static inline float segment2_length(const segment2_t segment) {
    return vector2_distance(segment.start, segment.end);
}

/**
 * @brief Test whether a point falls inside a segment's bounding box.
 *
 * Only meaningful for a point already known to be collinear with the segment,
 * where it distinguishes lying on the segment from lying on its extension.
 *
 * @param segment The input segment
 * @param point The point to test
 * @return True if the point is within the segment bounds.
 */
static inline bool segment2_contains_bounds(const segment2_t segment, const vector2_t point) {
    return (point.x <= MAXOF(segment.start.x, segment.end.x) + GEOMETRY_EPSILON)
        && (point.x >= MINOF(segment.start.x, segment.end.x) - GEOMETRY_EPSILON)
        && (point.y <= MAXOF(segment.start.y, segment.end.y) + GEOMETRY_EPSILON)
        && (point.y >= MINOF(segment.start.y, segment.end.y) - GEOMETRY_EPSILON);
}

/**
 * @brief Test whether two segments intersect, including collinear overlap.
 *
 * @param a The first segment
 * @param b The second segment
 * @return True if the segments touch or cross anywhere.
 */
static inline bool segment2_intersects(const segment2_t a, const segment2_t b) {
    winding_t b_start_vs_a = vector2_winding(a.start, a.end, b.start);
    winding_t b_end_vs_a   = vector2_winding(a.start, a.end, b.end);
    winding_t a_start_vs_b = vector2_winding(b.start, b.end, a.start);
    winding_t a_end_vs_b   = vector2_winding(b.start, b.end, a.end);

    // collinear endpoints only touch if they land within the other segment
    if (b_start_vs_a == WINDING_COLLINEAR && segment2_contains_bounds(a, b.start)) {
        return true;
    }
    if (b_end_vs_a == WINDING_COLLINEAR && segment2_contains_bounds(a, b.end)) {
        return true;
    }
    if (a_start_vs_b == WINDING_COLLINEAR && segment2_contains_bounds(b, a.start)) {
        return true;
    }
    if (a_end_vs_b == WINDING_COLLINEAR && segment2_contains_bounds(b, a.end)) {
        return true;
    }

    // otherwise they cross only if each segment straddles the other
    return (b_start_vs_a != b_end_vs_a) && (a_start_vs_b != a_end_vs_b);
}

/**
 * @brief Build the segment perpendicular to a segment through its midpoint.
 *
 * @param segment The segment to bisect
 * @param half_length Distance the result extends either side of the midpoint
 * @return The perpendicular bisector, degenerate if segment has no length.
 */
static inline segment2_t segment2_perpendicular_bisector(
    const segment2_t segment,
    const float half_length
) {
    vector2_t direction = vector2_sub(segment.end, segment.start);
    vector2_t offset = vector2_scale(
        vector2_normalize(vector2_perpendicular(direction)),
        half_length
    );
    vector2_t midpoint = vector2_midpoint(segment.start, segment.end);

    return (segment2_t){
        .start = vector2_sub(midpoint, offset),
        .end   = vector2_add(midpoint, offset),
    };
}

#endif // GEOMETRY_H
