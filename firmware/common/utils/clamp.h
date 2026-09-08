#ifndef CLAMP_H
#define CLAMP_H

/**
 * @file clamp.h
 * @brief Utility header to clamp a value to a bounded range
 *
 * Typesafe, supporting ints and floats.
 * Integer promotions also cover int8_t, uint8_t, int16_t, uint16_t
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#define DEFINE_CLAMP(type, name)                                      \
[[gnu::always_inline]]                                                \
static inline type clamp_##name(type input, type lower, type upper) { \
    if (input < lower) return lower;                                  \
    if (input > upper) return upper;                                  \
    return input;                                                     \
}

DEFINE_CLAMP(int, int)
DEFINE_CLAMP(unsigned int, uint)
DEFINE_CLAMP(long, long)
DEFINE_CLAMP(unsigned long, ulong)
DEFINE_CLAMP(float, float)

#define CLAMP(input, lower_bound, upper_bound) \
    (typeof(input))_Generic((input) + (lower_bound) + (upper_bound), \
        int: clamp_int, \
        unsigned int: clamp_uint, \
        long: clamp_long, \
        unsigned long: clamp_ulong, \
        float: clamp_float)(input, lower_bound, upper_bound)

#endif // CLAMP_H