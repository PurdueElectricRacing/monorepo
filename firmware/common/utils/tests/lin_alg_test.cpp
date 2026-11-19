/**
 * @file lin_alg_test.cpp
 * @brief Linear algebra functions unit tests
 *
 * @author Kerem Eyce (keyce@purdue.edu)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
    #include "lin_alg_test_shim.h"
}

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

static inline bool operator==(const matrix3x3_t& a, const matrix3x3_t& b) {
    return a.data[0][0] == b.data[0][0] && a.data[0][1] == b.data[0][1] && a.data[0][2] == b.data[0][2] && 
        a.data[1][0] == b.data[1][0] && a.data[1][1] == b.data[1][1] && a.data[1][2] == b.data[1][2] && 
        a.data[2][0] == b.data[2][0] && a.data[2][1] == b.data[2][1] && a.data[2][2] == b.data[2][2];
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

TEST(linear_algebra_test, magnitude_vector3)  {
    vector3_t vector1 = {1.f, 2.f, 2.f};
    EXPECT_FLOAT_EQ(test_vector3_magnitude(vector1), 3.f);
}

TEST(linear_algebra_test, normalize_vector3) {
    // test_vector3_normalize
    vector3_t vector1 = {1.f, 2.f, 2.f};
    vector3_t norm = test_vector3_normalize(vector1);
    EXPECT_FLOAT_EQ(norm.x, 1.f/3);
    EXPECT_FLOAT_EQ(norm.y, 2.f/3);
    EXPECT_FLOAT_EQ(norm.z, 2.f/3);

    vector3_t zero = {0.f, 0.f, 0.f};
    vector3_t norm_zero = test_vector3_normalize(zero);
    EXPECT_FLOAT_EQ(norm_zero.x, 0.f);
    EXPECT_FLOAT_EQ(norm_zero.y, 0.f);
    EXPECT_FLOAT_EQ(norm_zero.z, 0.f);
}

TEST(linear_algebra_test, matrix_multiply_vector3) {
    vector3_t input_vector = {1.f,2.f,3.f};
    matrix3x3_t input_matrix;
    input_matrix.data[0][0] = 1.f;
    input_matrix.data[0][1] = 2.f;
    input_matrix.data[0][2] = 3.f;
    input_matrix.data[1][0] = 4.f;
    input_matrix.data[1][1] = 5.f;
    input_matrix.data[1][2] = 6.f;
    input_matrix.data[2][0] = 7.f;
    input_matrix.data[2][1] = 8.f;
    input_matrix.data[2][2] = 9.f;
    vector3_t result = {14.f,32.f,50.f};
    EXPECT_EQ(test_matrix_multiply_vector3(&input_matrix, &input_vector), result);
}

TEST(linear_algebra_test, matrix_multiply_matrix3) {
    // test_matrix_multiply_matrix3x3
    matrix3x3_t matrix1;
    matrix1.data[0][0] = 1.f;
    matrix1.data[0][1] = 2.f;
    matrix1.data[0][2] = 3.f;
    matrix1.data[1][0] = 4.f;
    matrix1.data[1][1] = 5.f;
    matrix1.data[1][2] = 6.f;
    matrix1.data[2][0] = 7.f;
    matrix1.data[2][1] = 8.f;
    matrix1.data[2][2] = 9.f;

    matrix3x3_t matrix2;
    matrix2.data[0][0] = 9.f;
    matrix2.data[0][1] = 8.f;
    matrix2.data[0][2] = 7.f;
    matrix2.data[1][0] = 6.f;
    matrix2.data[1][1] = 5.f;
    matrix2.data[1][2] = 4.f;
    matrix2.data[2][0] = 3.f;
    matrix2.data[2][1] = 2.f;
    matrix2.data[2][2] = 1.f;

    matrix3x3_t result;
    result.data[0][0] = 30.f;
    result.data[0][1] = 24.f;
    result.data[0][2] = 18.f;
    result.data[1][0] = 84.f;
    result.data[1][1] = 69.f;
    result.data[1][2] = 54.f;
    result.data[2][0] = 138.f;
    result.data[2][1] = 114.f;
    result.data[2][2] = 90.f;

    EXPECT_EQ(test_matrix_multiply_matrix3x3(&matrix1, &matrix2), result);
}