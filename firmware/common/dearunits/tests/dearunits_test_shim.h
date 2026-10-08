#ifndef DEARUNITS_TEST_SHIM_H
#define DEARUNITS_TEST_SHIM_H

#include <stdbool.h>

float du_test_meters_from_feet(float x);
float du_test_feet_from_meters(float x);
float du_test_celsius_from_fahrenheit(float x);
float du_test_fahrenheit_from_celsius(float x);
float du_test_velocity(float a, float b);
float du_test_distance(float a, float b);
float du_test_ratio(float a, float b);
float du_test_scale(float a, float b);
float du_test_scale_left(float a, float b);
float du_test_scale_centimeters(float a, float b);
float du_test_divide_scalar(float a, float b);
float du_test_frequency(float x);
float du_test_period(float x);
float du_test_square(float x);
float du_test_sqrt(float x);
float du_test_force_constructor(float mass, float length, float time);
float du_test_add(float a, float b);
float du_test_subtract(float a, float b);
float du_test_min(float a, float b);
float du_test_max(float a, float b);
float du_test_abs_diff(float a, float b);
float du_test_negate(float x);
float du_test_abs(float x);
float du_test_clamp(float x, float lo, float hi);
bool du_test_lt(float a, float b);
bool du_test_gt(float a, float b);
bool du_test_le(float a, float b);
bool du_test_ge(float a, float b);
bool du_test_nearly_equal(float a, float b, float tol);

#endif // DEARUNITS_TEST_SHIM_H
