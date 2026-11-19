/**
 * @file matrix3.c
 * @brief Implementation of 3x3 matrix operations
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "linear_algebra.h"

#include <math.h>

vec3_t mat3_column(
    const mat3_t *mat,
    int col_index
) {
    return (vec3_t){
        mat->data[0][col_index],
        mat->data[1][col_index],
        mat->data[2][col_index]
    };
}

mat3_t outer_product3(
    const vec3_t a,
    const vec3_t b
) {
    mat3_t result = {0};
    result.data[0][0] = a.x * b.x;
    result.data[0][1] = a.x * b.y;
    result.data[0][2] = a.x * b.z;
    result.data[1][0] = a.y * b.x;
    result.data[1][1] = a.y * b.y;
    result.data[1][2] = a.y * b.z;
    result.data[2][0] = a.z * b.x;
    result.data[2][1] = a.z * b.y;
    result.data[2][2] = a.z * b.z;
    return result;
}

mat3_t mat3_add(
    const mat3_t *a,
    const mat3_t *b
) {
    mat3_t result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = a->data[i][j] + b->data[i][j];
        }
    }
    return result;
}

mat3_t mat3_sub(
    const mat3_t *a,
    const mat3_t *b
) {
    mat3_t result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = a->data[i][j] - b->data[i][j];
        }
    }
    return result;
}

vec3_t mat3_mult_vec3(
    const mat3_t *mat,
    const vec3_t *vec
) {
    vec3_t result = {0};
    result.x = mat->data[0][0] * vec->x + mat->data[0][1] * vec->y + mat->data[0][2] * vec->z;
    result.y = mat->data[1][0] * vec->x + mat->data[1][1] * vec->y + mat->data[1][2] * vec->z;
    result.z = mat->data[2][0] * vec->x + mat->data[2][1] * vec->y + mat->data[2][2] * vec->z;
    return result;
}

mat3_t mat3_mult(
    const mat3_t *a,
    const mat3_t *b
) {
    mat3_t result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = a->data[i][0] * b->data[0][j] +
                                a->data[i][1] * b->data[1][j] +
                                a->data[i][2] * b->data[2][j];
        }
    }
    return result;
}

mat3_t mat3_scale(
    const mat3_t *mat,
    float scalar
) {
    mat3_t result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = mat->data[i][j] * scalar;
        }
    }
    return result;
}


mat3_t cholesky3(
    const mat3_t *mat // ! must be SPD
) {
    mat3_t result = {0};

    // first column
    result.data[0][0] = sqrtf(mat->data[0][0]);
    result.data[1][0] = mat->data[1][0] / result.data[0][0];
    result.data[2][0] = mat->data[2][0] / result.data[0][0];

    // second column
    result.data[1][1] = sqrtf(
        mat->data[1][1] - result.data[1][0] * result.data[1][0]
    );
    result.data[2][1] = (
        mat->data[2][1] - result.data[2][0] * result.data[1][0]
    ) / result.data[1][1];

    // third column
    result.data[2][2] = sqrtf(
        mat->data[2][2]
        - result.data[2][0] * result.data[2][0]
        - result.data[2][1] * result.data[2][1]
    );

    return result;
}

mat3_t symmetrization3(
    const mat3_t *mat
) {
    mat3_t result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = (mat->data[i][j] + mat->data[j][i]) / 2.0f;
        }
    }
    return result;
}
