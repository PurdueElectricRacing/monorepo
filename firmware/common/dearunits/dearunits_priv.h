#ifndef DEARUNITS_PRIV_H
#define DEARUNITS_PRIV_H

/**
 * @file dearunits_priv.h
 * @brief Implementation details of dearunits.h; do not include directly.
 *
 * @author Danny Proano (dproano@purdue.edu)
 */

#define DU_FN [[nodiscard, gnu::always_inline]] static inline

#define DU_DEFINE_UNIT_OPERATIONS(U) \
    DU_FN U##_t dearunits_add_##U(U##_t a, U##_t b) { return (U##_t){ .value = a.value + b.value }; } \
    DU_FN U##_t dearunits_subtract_##U(U##_t a, U##_t b) { return (U##_t){ .value = a.value - b.value }; } \
    DU_FN U##_t dearunits_negate_##U(U##_t a) { return (U##_t){ .value = -a.value }; } \
    DU_FN U##_t dearunits_abs_##U(U##_t a) { return (U##_t){ .value = __builtin_fabsf(a.value) }; } \
    DU_FN U##_t dearunits_min_##U(U##_t a, U##_t b) { return (U##_t){ .value = __builtin_isunordered(a.value, b.value) ? __builtin_nanf("") : ((a.value < b.value) ? a.value : b.value) }; } \
    DU_FN U##_t dearunits_max_##U(U##_t a, U##_t b) { return (U##_t){ .value = __builtin_isunordered(a.value, b.value) ? __builtin_nanf("") : ((a.value > b.value) ? a.value : b.value) }; } \
    DU_FN U##_t dearunits_clamp_##U(U##_t x, U##_t lo, U##_t hi) { return (U##_t){ .value = (__builtin_isunordered(x.value, lo.value) || __builtin_isunordered(x.value, hi.value)) ? __builtin_nanf("") : ((x.value < lo.value) ? lo.value : ((x.value > hi.value) ? hi.value : x.value)) }; } \
    DU_FN bool dearunits_nearly_equal_##U(U##_t a, U##_t b, U##_t tol) { return __builtin_fabsf(a.value - b.value) <= __builtin_fabsf(tol.value); } \
    DU_FN bool dearunits_lt_##U(U##_t a, U##_t b) { return a.value < b.value; } \
    DU_FN bool dearunits_gt_##U(U##_t a, U##_t b) { return a.value > b.value; } \
    DU_FN bool dearunits_le_##U(U##_t a, U##_t b) { return a.value <= b.value; } \
    DU_FN bool dearunits_ge_##U(U##_t a, U##_t b) { return a.value >= b.value; }

#define DU_DEFINE_MULTIPLY(L, R, RES) \
    DU_FN RES##_t dearunits_multiply_##L##_by_##R(L##_t a, R##_t b) { return (RES##_t){ .value = a.value * b.value }; }
#define DU_DEFINE_DIVIDE(L, R, RES) \
    DU_FN RES##_t dearunits_divide_##L##_by_##R(L##_t a, R##_t b) { return (RES##_t){ .value = a.value / b.value }; }

#define DU_DEFINE_MULTIPLY_TO_FLOAT(L, R) \
    DU_FN float dearunits_multiply_##L##_by_##R(L##_t a, R##_t b) { return a.value * b.value; }
#define DU_DEFINE_DIVIDE_TO_FLOAT(L, R) \
    DU_FN float dearunits_divide_##L##_by_##R(L##_t a, R##_t b) { return a.value / b.value; }

#define DU_DEFINE_SCALAR_OPS(U) \
    DU_FN U##_t dearunits_multiply_##U##_by_scalar(U##_t a, float b) { return (U##_t){ .value = a.value * b }; } \
    DU_FN U##_t dearunits_divide_##U##_by_scalar(U##_t a, float b) { return (U##_t){ .value = a.value / b }; } \
    DU_FN U##_t dearunits_multiply_scalar_by_##U(float a, U##_t b) { return (U##_t){ .value = a * b.value }; }

#define DU_DEFINE_INVERSE(U, RES) \
    DU_FN RES##_t dearunits_divide_scalar_by_##U(float a, U##_t b) { return (RES##_t){ .value = a / b.value }; }

#endif // DEARUNITS_PRIV_H
