#ifndef LIN_ALG_TEST_SHIM_H
#define LIN_ALG_TEST_SHIM_H

#include "linear_algebra.h"

vector2_t test_vector2_add(const vector2_t a, const vector2_t b);
vector2_t test_vector2_sub(const vector2_t a, const vector2_t b);
vector2_t test_vector2_scale(const vector2_t vec, const float scalar);
float test_vector2_dot(const vector2_t a, const vector2_t b);
float test_vector2_cross(const vector2_t a, const vector2_t b);
float test_vector2_magnitude(const vector2_t vec);
vector2_t test_vector2_normalize(const vector2_t vec);
float test_vector2_distance(const vector2_t a, const vector2_t b);
vector2_t test_vector2_midpoint(const vector2_t a, const vector2_t b);
vector2_t test_vector2_perpendicular(const vector2_t vec);

float test_vector3_magnitude(const vector3_t vec);
vector3_t test_vector3_normalize(const vector3_t vec);

vector3_t test_matrix_multiply_vector3(const matrix3x3_t *mat, const vector3_t *in);
matrix3x3_t test_matrix_multiply_matrix3x3(const matrix3x3_t *a, const matrix3x3_t *b);

#endif // LIN_ALG_TEST_SHIM_H
