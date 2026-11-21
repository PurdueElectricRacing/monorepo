/**
 * @file orientation_test.cpp
 * @brief orientation unit test
 *
 * @author Kerem Eyce (keyce@purdue.edu)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h> // 1. Include GoogleMock
#include <cstring>
#include <cstdint>
#include <cmath>

extern "C" {
    #include "orientation_test_shim.h"
}

inline constexpr bool floats_equal(float a, float b, float epsilon = 0.00001f) {
    return std::abs(a - b) < epsilon;
}

static inline bool operator==(const mat3_t& a, const mat3_t& b) {
    return floats_equal(a[0][0], b[0][0]) && floats_equal(a[0][1], b[0][1]) && floats_equal(a[0][2], b[0][2]) &&
        floats_equal(a[1][0], b[1][0]) && floats_equal(a[1][1], b[1][1]) && floats_equal(a[1][2], b[1][2]) && 
        floats_equal(a[2][0], b[2][0]) && floats_equal(a[2][1], b[2][1]) && floats_equal(a[2][2], b[2][2]);
}

/*
typedef struct {
    float data[3][3];
} mat3_t;
*/


TEST(orientation_test, DCM_from_euler) {
    euler_angles_t test_angles = {
        .roll  = 0.52359877559f,     // (pi/6 radians)
        .pitch = 0.52359877559f,     // (pi/6 radians)
        .yaw   = 1.04719755120f      // (pi/3 radians)
    };

    mat3_t result_matrix = test_DCM_from_euler(angles);

    mat3_t expected_dcm = {
        .data = {
            {  0.4330127f, -0.6250000f,  0.6495190f},
            {  0.7500000f,  0.6495190f,  0.1250000f},
            { -0.5000000f,  0.4330127f,  0.7500000f}
        }
    };


    EXPECT_EQ(result_matrix, expected_dcm);
}