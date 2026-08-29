#ifndef MAX_H
#define MAX_H

/**
 * @file max.h
 * @brief Maximum value functions utility header.
 *
 * Typesafe, supporting ints and floats.
 * Generic MAXOF interface for 2, 3, or 4 arguments.
 * Integer promotions also cover int8_t, uint8_t, int16_t, uint16_t
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

[[gnu::always_inline]]
static inline signed int max2_signed(signed int a, signed int b) {
    return (a > b) ? a : b;
}

[[gnu::always_inline]]
static inline unsigned int max2_unsigned(unsigned int a, unsigned int b) {
    return (a > b) ? a : b;
}

[[gnu::always_inline]]
static inline float max2_float(float a, float b) {
    return (a > b) ? a : b;
}

#define MAXOF2(a, b) (typeof(a))_Generic((a) + (b), \
    signed int: max2_signed, \
    unsigned int: max2_unsigned, \
    float: max2_float \
)(a, b)

#define MAXOF3(a, b, c) MAXOF2(MAXOF2(a, b), c)
#define MAXOF4(a, b, c, d) MAXOF2(MAXOF2(a, b), MAXOF2(c, d))
// if you need more than four arguments, just add them here

// dispatcher macro selects the correct MAXOF function based on the number of arguments
#define DISPATCH_MAXOF(_1, _2, _3, _4, NAME, ...) NAME
#define MAXOF(...) DISPATCH_MAXOF(__VA_ARGS__, MAXOF4, MAXOF3, MAXOF2)(__VA_ARGS__)

#endif // MAX_H