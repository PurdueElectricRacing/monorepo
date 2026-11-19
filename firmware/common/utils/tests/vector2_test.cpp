/**
 * @file vector2_test.cpp
 * @brief 2D vector functions unit tests
 *
 * @author Kerem Eyce (keyce@purdue.edu)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
    #include "vector2_test_shim.h"
}

// Define operator== and operator<< for GoogleTest assertions
static inline bool operator==(const vector2_t& a, const vector2_t& b) {
    return a.x == b.x && a.y == b.y;
}

static inline std::ostream& operator<<(std::ostream& os, const vector2_t& v) {
    return os << "vector2_t{" << v.x << ", " << v.y << "}";
}

TEST(vector2_test, add_vector2) {
    vector2_t vector1 = {10.f, 15.f};
    vector2_t vector2 = {2.f, -2.f};
    vector2_t result = {12.f, 13.f};
    EXPECT_EQ(test_vector2_add(vector1, vector2), result);
}

TEST(vector2_test, sub_vector2) {
    vector2_t vector1 = {10.f, 15.f};
    vector2_t vector2 = {2.f, -2.f};
    vector2_t result = {8.f, 17.f};
    EXPECT_EQ(test_vector2_sub(vector1, vector2), result);
}

TEST(vector2_test, scale_vector2) {
    vector2_t vector1 = {3.f, -4.f};
    vector2_t result = {6.f, -8.f};
    EXPECT_EQ(test_vector2_scale(vector1, 2.f), result);
}

TEST(vector2_test, dot_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    vector2_t vector2 = {2.f, -1.f};
    EXPECT_FLOAT_EQ(test_vector2_dot(vector1, vector2), 2.f);
}

TEST(vector2_test, magnitude_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    EXPECT_FLOAT_EQ(test_vector2_magnitude(vector1), 5.f);
}

TEST(vector2_test, cross_vector2) {
    EXPECT_FLOAT_EQ(test_vector2_cross({3.f, 4.f}, {2.f, -1.f}), -11.f);
}

TEST(vector2_test, distance_vector2) {
    EXPECT_FLOAT_EQ(test_vector2_distance({0.f, 0.f}, {3.f, 4.f}), 5.f);
}

TEST(vector2_test, midpoint_vector2) {
    vector2_t expected = {1.f, 2.f};
    EXPECT_EQ(test_vector2_midpoint({0.f, 0.f}, {2.f, 4.f}), expected);
}

TEST(vector2_test, perpendicular_vector2) {
    vector2_t expected = {-2.f, 1.f};
    EXPECT_EQ(test_vector2_perpendicular({1.f, 2.f}), expected);
}

TEST(vector2_test, normalize_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    vector2_t norm = test_vector2_normalize(vector1);
    EXPECT_FLOAT_EQ(norm.x, 0.6f);
    EXPECT_FLOAT_EQ(norm.y, 0.8f);

    vector2_t zero = {0.f, 0.f};
    vector2_t norm_zero = test_vector2_normalize(zero);
    EXPECT_FLOAT_EQ(norm_zero.x, 0.f);
    EXPECT_FLOAT_EQ(norm_zero.y, 0.f);
}
