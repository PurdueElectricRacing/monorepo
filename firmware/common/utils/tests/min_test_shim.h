#ifndef MIN_TEST_SHIM_H
#define MIN_TEST_SHIM_H

#include <stdint.h>

signed int test_min2_signed(signed int a, signed int b);
unsigned int test_min2_unsigned(unsigned int a, unsigned int b);
float test_min2_float(float a, float b);

int test_MINOF2_int(int a, int b);
unsigned int test_MINOF2_uint(unsigned int a, unsigned int b);
float test_MINOF2_float(float a, float b);

int test_MINOF3_int(int a, int b, int c);
float test_MINOF3_float(float a, float b, float c);
int test_MINOF4_int(int a, int b, int c, int d);

#endif // MIN_TEST_SHIM_H
