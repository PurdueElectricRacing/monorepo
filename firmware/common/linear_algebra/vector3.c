/**
 * @file vector3.c
 * @brief Implementation of 3D vector operations
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "linear_algebra.h"

#include <math.h>

vec3_t vec3_add(
    const vec3_t a,
    const vec3_t b
) {
    return (vec3_t) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

vec3_t vec3_sub(
    const vec3_t a,
    const vec3_t b
) {
    return (vec3_t) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

vec3_t vec3_scale(
    const vec3_t vec,
    float scalar
) {
    return (vec3_t) {
        vec.x * scalar,
        vec.y * scalar,
        vec.z * scalar
    };
}

float vec3_mag(const vec3_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

vec3_t vec3_normalize(const vec3_t vec) {
    float mag = vec3_mag(vec);
    if (mag < 0.0001f) {
        return (vec3_t){0, 0, 0};
    }

    return vec3_scale(vec, 1.0f / mag);
}
