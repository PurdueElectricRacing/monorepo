#ifndef VECTOR2_TEST_SHIM_H
#define VECTOR2_TEST_SHIM_H

#include "vector2.h"

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

#endif // VECTOR2_TEST_SHIM_H
