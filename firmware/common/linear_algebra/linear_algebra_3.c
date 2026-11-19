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

vec3_t vec3_normalize(const vec3_t vec) {
    float mag = vec3_mag(vec);
    if (mag < 0.0001f) {
        return (vec3_t){0, 0, 0};
    }

    return (vec3_t){
        vec.x / mag,
        vec.y / mag,
        vec.z / mag
    };
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
    const mat3_t *mat
) {
    mat3_t result = {0};



    return result;
}