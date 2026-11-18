/**
 * @file lin_alg_test_shim.c
 * @brief Test shim to compile linear_algebra functions as C23.
 */

#include "lin_alg_test_shim.h"

vector2_t test_vector2_add(const vector2_t a, const vector2_t b) {
    return vector2_add(a, b);
}

vector2_t test_vector2_sub(const vector2_t a, const vector2_t b) {
    return vector2_sub(a, b);
}

vector2_t test_vector2_scale(const vector2_t vec, const float scalar) {
    return vector2_scale(vec, scalar);
}

float test_vector2_dot(const vector2_t a, const vector2_t b) {
    return vector2_dot(a, b);
}

float test_vector2_cross(const vector2_t a, const vector2_t b) {
    return vector2_cross(a, b);
}

float test_vector2_magnitude(const vector2_t vec) {
    return vector2_magnitude(vec);
}

vector2_t test_vector2_normalize(const vector2_t vec) {
    return vector2_normalize(vec);
}

float test_vector2_distance(const vector2_t a, const vector2_t b) {
    return vector2_distance(a, b);
}

vector2_t test_vector2_midpoint(const vector2_t a, const vector2_t b) {
    return vector2_midpoint(a, b);
}

vector2_t test_vector2_perpendicular(const vector2_t vec) {
    return vector2_perpendicular(vec);
}

float test_vector3_magnitude(const vector3_t vec) {
    return vector3_magnitude(vec);
}

vector3_t test_vector3_normalize(const vector3_t vec) {
    return vector3_normalize(vec);
}

vector3_t test_matrix_multiply_vector3(const matrix3x3_t *mat, const vector3_t *in) {
    return matrix_multiply_vector3(mat, in);
}

matrix3x3_t test_matrix_multiply_matrix3x3(const matrix3x3_t *a, const matrix3x3_t *b) {
    return matrix_multiply_matrix3x3(a, b);
}
