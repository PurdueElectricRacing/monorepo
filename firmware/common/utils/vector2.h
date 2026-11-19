#ifndef VECTOR2_H
#define VECTOR2_H

/**
 * @file vector2.h
 * @brief 2D vector types and operations.
 *
 * Basic floating point 2D vector operations.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <math.h>

typedef struct {
    float x;
    float y;
} vector2_t;

/**
 * @brief Add two 2D vectors.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The sum a + b.
 */
static inline vector2_t vector2_add(const vector2_t a, const vector2_t b) {
    return (vector2_t){a.x + b.x, a.y + b.y};
}

/**
 * @brief Subtract one 2D vector from another.
 *
 * @param a The vector to subtract from
 * @param b The vector to subtract
 * @return The difference a - b.
 */
static inline vector2_t vector2_sub(const vector2_t a, const vector2_t b) {
    return (vector2_t){a.x - b.x, a.y - b.y};
}

/**
 * @brief Scale a 2D vector by a scalar.
 *
 * @param vec The input vector
 * @param scalar The value to scale by
 * @return The scaled vector.
 */
static inline vector2_t vector2_scale(const vector2_t vec, const float scalar) {
    return (vector2_t){vec.x * scalar, vec.y * scalar};
}

/**
 * @brief Dot product of two 2D vectors.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The scalar dot product.
 */
static inline float vector2_dot(const vector2_t a, const vector2_t b) {
    return (a.x * b.x) + (a.y * b.y);
}

/**
 * @brief Cross product of two 2D vectors.
 *
 * In 2D the cross product has only a Z component, so it reduces to a scalar.
 * Positive means b lies counterclockwise of a, negative means clockwise.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The Z component of a x b.
 */
static inline float vector2_cross(const vector2_t a, const vector2_t b) {
    return (a.x * b.y) - (a.y * b.x);
}

/**
 * @brief Calculate the magnitude of a 2D vector.
 *
 * @param vec The input vector
 * @return The magnitude of the vector.
 */
static inline float vector2_magnitude(const vector2_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);
}

/**
 * @brief Normalize a 2D vector.
 *
 * @param vec The input vector
 * @return The normalized vector, or the zero vector if vec is degenerate.
 */
static inline vector2_t vector2_normalize(const vector2_t vec) {
    float mag = vector2_magnitude(vec);
    if (mag < 0.0001f) {
        return (vector2_t){0, 0};
    }

    return (vector2_t){
        vec.x / mag,
        vec.y / mag
    };
}

/**
 * @brief Distance between two 2D points.
 *
 * @param a The first point
 * @param b The second point
 * @return The straight line distance between a and b.
 */
static inline float vector2_distance(const vector2_t a, const vector2_t b) {
    return vector2_magnitude(vector2_sub(a, b));
}

/**
 * @brief Midpoint of two 2D points.
 *
 * @param a The first point
 * @param b The second point
 * @return The point halfway between a and b.
 */
static inline vector2_t vector2_midpoint(const vector2_t a, const vector2_t b) {
    return vector2_scale(vector2_add(a, b), 0.5f);
}

/**
 * @brief Rotate a 2D vector 90 degrees counterclockwise.
 *
 * @param vec The input vector
 * @return A vector of equal magnitude perpendicular to vec.
 */
static inline vector2_t vector2_perpendicular(const vector2_t vec) {
    return (vector2_t){-vec.y, vec.x};
}

#endif // VECTOR2_H
