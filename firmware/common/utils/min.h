#ifndef MIN_H
#define MIN_H

/**
 * @file min.h
 * @brief Minimum value functions utility header.
 *
 * Typesafe, supporting ints and floats.
 * Generic MINOF interface for 2, 3, or 4 arguments.
 * Integer promotions also cover int8_t, uint8_t, int16_t, uint16_t
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

[[gnu::always_inline]]
static inline signed int min2_signed(signed int a, signed int b) {
    return (a < b) ? a : b;
}

[[gnu::always_inline]]
static inline unsigned int min2_unsigned(unsigned int a, unsigned int b) {
    return (a < b) ? a : b;
}

[[gnu::always_inline]]
static inline float min2_float(float a, float b) {
    return (a < b) ? a : b;
}

#define MINOF2(a, b) (typeof(a))_Generic((a) + (b), \
    signed int: min2_signed, \
    unsigned int: min2_unsigned, \
    float: min2_float \
)(a, b)

#define MINOF3(a, b, c) MINOF2(MINOF2(a, b), c)
#define MINOF4(a, b, c, d) MINOF2(MINOF2(a, b), MINOF2(c, d))
// if you need more than four arguments, just add them here

// dispatcher macro selects the correct MINOF function based on the number of arguments
#define DISPATCH_MINOF(_1, _2, _3, _4, NAME, ...) NAME
#define MINOF(...) DISPATCH_MINOF(__VA_ARGS__, MINOF4, MINOF3, MINOF2)(__VA_ARGS__)

#endif // MIN_H