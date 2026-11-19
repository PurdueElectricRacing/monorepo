#include <gtest/gtest.h>

extern "C" {
#include "linear_algebra.h"
}

static void expect_matrix(const mat3_t &actual, const mat3_t &expected) {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            EXPECT_NEAR(actual.data[row][col], expected.data[row][col], 1e-5f)
                << "row " << row << ", column " << col;
        }
    }
}

TEST(Matrix3, ArithmeticAndProducts) {
    const mat3_t a = {{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
    }};
    const mat3_t b = {{
        {9, 8, 7},
        {6, 5, 4},
        {3, 2, 1},
    }};
    expect_matrix(mat3_add(&a, &b), {{
        {10, 10, 10},
        {10, 10, 10},
        {10, 10, 10},
    }});
    expect_matrix(mat3_sub(&a, &b), {{
        {-8, -6, -4},
        {-2, 0, 2},
        {4, 6, 8},
    }});
    expect_matrix(mat3_scale(&a, 2), {{
        {2, 4, 6},
        {8, 10, 12},
        {14, 16, 18},
    }});
    expect_matrix(mat3_mult(&a, &b), {{
        {30, 24, 18},
        {84, 69, 54},
        {138, 114, 90},
    }});

    const vec3_t v = {1, 2, 3};
    const vec3_t product = mat3_mult_vec3(&a, &v);
    EXPECT_FLOAT_EQ(product.x, 14);
    EXPECT_FLOAT_EQ(product.y, 32);
    EXPECT_FLOAT_EQ(product.z, 50);

    expect_matrix(outer_product3(v, {4, 5, 6}), {{
        {4, 5, 6},
        {8, 10, 12},
        {12, 15, 18},
    }});
}

TEST(Matrix3, ColumnsAndSymmetrization) {
    const mat3_t a = {{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
    }};

    for (int col = 0; col < 3; ++col) {
        const vec3_t column = mat3_column(&a, col);
        EXPECT_FLOAT_EQ(column.x, a.data[0][col]);
        EXPECT_FLOAT_EQ(column.y, a.data[1][col]);
        EXPECT_FLOAT_EQ(column.z, a.data[2][col]);
    }

    expect_matrix(symmetrization3(&a), {{
        {1, 3, 5},
        {3, 5, 7},
        {5, 7, 9},
    }});
}

TEST(Matrix3, Cholesky) {
    const mat3_t identity = {{
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
    }};
    expect_matrix(cholesky3(&identity), identity);

    const mat3_t diagonal = {{
        {4, 0, 0},
        {0, 9, 0},
        {0, 0, 16},
    }};
    expect_matrix(cholesky3(&diagonal), {{
        {2, 0, 0},
        {0, 3, 0},
        {0, 0, 4},
    }});

    const mat3_t input = {{
        {4, 2, 6},
        {2, 10, 6},
        {6, 6, 26},
    }};
    const mat3_t expected = {{
        {2, 0, 0},
        {1, 3, 0},
        {3, 1, 4},
    }};
    const mat3_t factor = cholesky3(&input);
    expect_matrix(factor, expected);

    mat3_t transpose = {};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            transpose.data[row][col] = factor.data[col][row];
        }
    }

    expect_matrix(mat3_mult(&factor, &transpose), input);
}
