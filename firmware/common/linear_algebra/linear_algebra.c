/**
 * @file linear_algebra.c
 * @brief Basic floating point vector and matrix types and operations
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <math.h>

typedef struct {
    float x;
    float y;
    float z;
} vector3_t;

typedef struct {
    float data[3][3];
} matrix3x3_t;

float vector3_magnitude(const vector3_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

vector3_t vector3_normalize(const vector3_t vec) {
    float mag = vector3_magnitude(vec);
    if (mag < 0.0001f) {
        return (vector3_t){0, 0, 0};
    }

    return (vector3_t){
        vec.x / mag,
        vec.y / mag,
        vec.z / mag
    };
}

vector3_t matrix_multiply_vector3(const matrix3x3_t *mat, const vector3_t *in) {
    vector3_t out;
    out.x = mat->data[0][0] * in->x + mat->data[0][1] * in->y + mat->data[0][2] * in->z;
    out.y = mat->data[1][0] * in->x + mat->data[1][1] * in->y + mat->data[1][2] * in->z;
    out.z = mat->data[2][0] * in->x + mat->data[2][1] * in->y + mat->data[2][2] * in->z;
    return out;
}

matrix3x3_t matrix_multiply_matrix3x3(const matrix3x3_t *a, const matrix3x3_t *b) {
    matrix3x3_t result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = a->data[i][0] * b->data[0][j] +
                                a->data[i][1] * b->data[1][j] +
                                a->data[i][2] * b->data[2][j];
        }
    }
    return result;
}

matrix3x3_t matrix_multiply_scalar(const matrix3x3_t *mat, float scalar) {
    matrix3x3_t result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.data[i][j] = mat->data[i][j] * scalar;
        }
    }
    return result;
}
