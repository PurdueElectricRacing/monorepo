#ifndef LINEAR_ALGEBRA_H
#define LINEAR_ALGEBRA_H

/**
 * @file linear_algebra.h
 * @brief Linear algebra functions utility header.
 *
 * Basic floating point vector and matrix types and operations.
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

/**
 * @brief Calculate the magnitude of a 3D vector.
 *
 * @param vec The input vector
 * @return The magnitude of the vector.
 */
static inline float vector3_magnitude(const vector3_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

/**
 * @brief Normalize a 3D vector.
 *
 * @param vec The input vector
 * @return The normalized vector.
 */
static inline vector3_t vector3_normalize(const vector3_t vec) {
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

/**
 * @brief Multiply a 3x3 matrix with a 3D vector.
 *
 * @param mat The input matrix
 * @param in The input vector
 * @return The resulting vector.
 */
static inline vector3_t matrix_multiply_vector3(const matrix3x3_t *mat, const vector3_t *in) {
    vector3_t out;
    out.x = mat->data[0][0] * in->x + mat->data[0][1] * in->y + mat->data[0][2] * in->z;
    out.y = mat->data[1][0] * in->x + mat->data[1][1] * in->y + mat->data[1][2] * in->z;
    out.z = mat->data[2][0] * in->x + mat->data[2][1] * in->y + mat->data[2][2] * in->z;
    return out;
}

/**
 * @brief Multiply a 3x3 matrix with another 3x3 matrix.
 *
 * @param a The first input matrix
 * @param b The second input matrix
 * @return The resulting matrix AB.
 */
static inline matrix3x3_t matrix_multiply_matrix3x3(const matrix3x3_t *a, const matrix3x3_t *b) {
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

#endif // LINEAR_ALGEBRA_H
