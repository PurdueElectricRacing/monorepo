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
} vector2_t;

typedef struct {
    float x;
    float y;
    float z;
} vector3_t;

typedef struct {
    float data[3][3];
} matrix3x3_t;

/**
 * @brief Add two 2D vectors.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The sum a + b.
 */
static inline vector2_t vector2_add(const vector2_t a, const vector2_t b) {
    return (vector2_t){a.x + b.x, a.y + b.y};
}

/**
 * @brief Subtract one 2D vector from another.
 *
 * @param a The vector to subtract from
 * @param b The vector to subtract
 * @return The difference a - b.
 */
static inline vector2_t vector2_sub(const vector2_t a, const vector2_t b) {
    return (vector2_t){a.x - b.x, a.y - b.y};
}

/**
 * @brief Scale a 2D vector by a scalar.
 *
 * @param vec The input vector
 * @param scalar The value to scale by
 * @return The scaled vector.
 */
static inline vector2_t vector2_scale(const vector2_t vec, const float scalar) {
    return (vector2_t){vec.x * scalar, vec.y * scalar};
}

/**
 * @brief Dot product of two 2D vectors.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The scalar dot product.
 */
static inline float vector2_dot(const vector2_t a, const vector2_t b) {
    return (a.x * b.x) + (a.y * b.y);
}

/**
 * @brief Cross product of two 2D vectors.
 *
 * In 2D the cross product has only a Z component, so it reduces to a scalar.
 * Positive means b lies counterclockwise of a, negative means clockwise.
 *
 * @param a The first input vector
 * @param b The second input vector
 * @return The Z component of a x b.
 */
static inline float vector2_cross(const vector2_t a, const vector2_t b) {
    return (a.x * b.y) - (a.y * b.x);
}

/**
 * @brief Calculate the magnitude of a 2D vector.
 *
 * @param vec The input vector
 * @return The magnitude of the vector.
 */
static inline float vector2_magnitude(const vector2_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);
}

/**
 * @brief Normalize a 2D vector.
 *
 * @param vec The input vector
 * @return The normalized vector, or the zero vector if vec is degenerate.
 */
static inline vector2_t vector2_normalize(const vector2_t vec) {
    float mag = vector2_magnitude(vec);
    if (mag < 0.0001f) {
        return (vector2_t){0, 0};
    }

    return (vector2_t){
        vec.x / mag,
        vec.y / mag
    };
}

/**
 * @brief Distance between two 2D points.
 *
 * @param a The first point
 * @param b The second point
 * @return The straight line distance between a and b.
 */
static inline float vector2_distance(const vector2_t a, const vector2_t b) {
    return vector2_magnitude(vector2_sub(a, b));
}

/**
 * @brief Midpoint of two 2D points.
 *
 * @param a The first point
 * @param b The second point
 * @return The point halfway between a and b.
 */
static inline vector2_t vector2_midpoint(const vector2_t a, const vector2_t b) {
    return vector2_scale(vector2_add(a, b), 0.5f);
}

/**
 * @brief Rotate a 2D vector 90 degrees counterclockwise.
 *
 * @param vec The input vector
 * @return A vector of equal magnitude perpendicular to vec.
 */
static inline vector2_t vector2_perpendicular(const vector2_t vec) {
    return (vector2_t){-vec.y, vec.x};
}

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
