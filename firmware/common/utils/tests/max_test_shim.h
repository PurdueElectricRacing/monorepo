#ifndef MAX_TEST_SHIM_H
#define MAX_TEST_SHIM_H

#include <stdint.h>



signed int test_max2_signed(signed int a, signed int b);
unsigned int test_max2_unsigned(unsigned int a, unsigned int b);
float test_max2_float(float a, float b);

int test_MAXOF2_int(int a, int b);
unsigned int test_MAXOF2_uint(unsigned int a, unsigned int b);
float test_MAXOF2_float(float a, float b);

int test_MAXOF3_int(int a, int b, int c);
float test_MAXOF3_float(float a, float b, float c);
int test_MAXOF4_int(int a, int b, int c, int d);

int test_MAXOF_4arg(int a, int b, int c, int d);
int test_MAXOF_3arg(int a, int b, int c);
int test_MAXOF_2arg(int a, int b);

#endif  // MAX_TEST_SHIM_H
