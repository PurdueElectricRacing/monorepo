/**
 * @file linear_algebra.h
 * @brief Basic floating point vector and matrix types and operations
 *
 * ! Please keep this file minimal and dumb
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef LINEAR_ALGEBRA_H
#define LINEAR_ALGEBRA_H

typedef struct {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct {
    float data[3][3];
} mat3_t;

float vec3_mag(const vec3_t vec);

vec3_t vec3_scale(
    const vec3_t vec,
    float scalar
);

vec3_t vec3_add(
    const vec3_t a,
    const vec3_t b
);

vec3_t vec3_sub(
    const vec3_t a,
    const vec3_t b
);

vec3_t vec3_normalize(const vec3_t vec);

vec3_t mat3_column(
    const mat3_t *mat,
    int col_index
);

mat3_t outer_product3(
    const vec3_t a,
    const vec3_t b
);

mat3_t mat3_add(
    const mat3_t *a,
    const mat3_t *b
);

mat3_t mat3_sub(
    const mat3_t *a,
    const mat3_t *b
);

vec3_t mat3_mult_vec3(
    const mat3_t *mat,
    const vec3_t *vec
);

mat3_t mat3_mult(
    const mat3_t *a,
    const mat3_t *b
);

mat3_t mat3_scale(
    const mat3_t *mat,
    float scalar
);

mat3_t cholesky3(
    const mat3_t *mat
);

mat3_t symmetrization3(
    const mat3_t *mat
);

#endif // LINEAR_ALGEBRA_H
