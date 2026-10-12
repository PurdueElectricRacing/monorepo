/**
 * @file min_test_shim.c
 * @brief C-compilation environment for testing C23 min.h macros.
 */

#include "min_test_shim.h"
#include "min.h"

signed int test_min2_signed(signed int a, signed int b) {
    return min2_signed(a, b);
}

unsigned int test_min2_unsigned(unsigned int a, unsigned int b) {
    return min2_unsigned(a, b);
}

float test_min2_float(float a, float b) {
    return min2_float(a, b);
}

int test_MINOF2_int(int a, int b) {
    return MINOF2(a, b);
}

unsigned int test_MINOF2_uint(unsigned int a, unsigned int b) {
    return MINOF2(a, b);
}

float test_MINOF2_float(float a, float b) {
    return MINOF2(a, b);
}

int test_MINOF3_int(int a, int b, int c) {
    return MINOF3(a, b, c);
}

float test_MINOF3_float(float a, float b, float c) {
    return MINOF3(a, b, c);
}

int test_MINOF4_int(int a, int b, int c, int d) {
    return MINOF4(a, b, c, d);
}
