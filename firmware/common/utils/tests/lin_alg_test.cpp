/**
 * @file lin_alg_test.cpp
 * @brief Linear algebra functions unit tests
 *
 * @author Kerem Eyce (keyce@purdue.edu)
 */

#include <gtest/gtest.h>

#include "lin_alg_test_shim.h"

// Define operator== and operator<< for GoogleTest assertions
static inline bool operator==(const vector2_t& a, const vector2_t& b) {
    return a.x == b.x && a.y == b.y;
}

static inline std::ostream& operator<<(std::ostream& os, const vector2_t& v) {
    return os << "vector2_t{" << v.x << ", " << v.y << "}";
}

static inline bool operator==(const vector3_t& a, const vector3_t& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static inline std::ostream& operator<<(std::ostream& os, const vector3_t& v) {
    return os << "vector3_t{" << v.x << ", " << v.y << ", " << v.z << "}";
}

TEST(linear_algebra_test, add_vector2) {
    vector2_t vector1 = {10.f, 15.f};
    vector2_t vector2 = {2.f, -2.f};
    vector2_t result = {12.f, 13.f};
    EXPECT_EQ(test_vector2_add(vector1, vector2), result);
}

TEST(linear_algebra_test, sub_vector2) {
    vector2_t vector1 = {10.f, 15.f};
    vector2_t vector2 = {2.f, -2.f};
    vector2_t result = {8.f, 17.f};
    EXPECT_EQ(test_vector2_sub(vector1, vector2), result);
}

TEST(linear_algebra_test, scale_vector2) {
    vector2_t vector1 = {3.f, -4.f};
    vector2_t result = {6.f, -8.f};
    EXPECT_EQ(test_vector2_scale(vector1, 2.f), result);
}

TEST(linear_algebra_test, dot_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    vector2_t vector2 = {2.f, -1.f};
    EXPECT_FLOAT_EQ(test_vector2_dot(vector1, vector2), 2.f);
}

TEST(linear_algebra_test, magnitude_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    EXPECT_FLOAT_EQ(test_vector2_magnitude(vector1), 5.f);
}

TEST(linear_algebra_test, normalize_vector2) {
    vector2_t vector1 = {3.f, 4.f};
    vector2_t norm = test_vector2_normalize(vector1);
    EXPECT_FLOAT_EQ(norm.x, 0.6f);
    EXPECT_FLOAT_EQ(norm.y, 0.8f);

    vector2_t zero = {0.f, 0.f};
    vector2_t norm_zero = test_vector2_normalize(zero);
    EXPECT_FLOAT_EQ(norm_zero.x, 0.f);
    EXPECT_FLOAT_EQ(norm_zero.y, 0.f);
}
