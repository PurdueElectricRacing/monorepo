/**
 * @file linear_algebra_3.c
 * @brief Basic floating point vector and matrix types and operations
 *
 * Please keep this file minimal and dumb !
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <math.h>

typedef struct {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct {
    float data[3][3];
} mat3_t;

float vec3_mag(const vec3_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

vec3_t vec3_scale(
    const vec3_t vec,
    float scalar
) {
    return (vec3_t) {
        vec.x * scalar,
        vec.y * scalar,
        vec.z * scalar
    };
}

vec3_t vec3_add(
    const vec3_t a,
    const vec3_t b
) {
    return (vec3_t) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

vec3_t vec3_sub(
    const vec3_t a,
    const vec3_t b
) {
    return (vec3_t) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

vec3_t vec3_normalize(const vec3_t vec) {
    float mag = vec3_mag(vec);
    if (mag < 0.0001f) {
        return (vec3_t){0, 0, 0};
    }

    return vec3_scale(vec, 1.0f / mag);
}

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
