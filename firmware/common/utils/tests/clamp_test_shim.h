#ifndef CLAMP_TEST_SHIM_H
#define CLAMP_TEST_SHIM_H

#include <stdint.h>

int test_CLAMP_int(int input, int lower_bound, int upper_bound);
int8_t test_CLAMP_int8(int8_t input, int8_t lower_bound, int8_t upper_bound);
int16_t test_CLAMP_int16(int16_t input, int16_t lower_bound, int16_t upper_bound);
unsigned int test_CLAMP_uint(unsigned int input, unsigned int lower_bound, unsigned int upper_bound);
uint8_t test_CLAMP_uint8(uint8_t input, uint8_t lower_bound, uint8_t upper_bound);
uint16_t test_CLAMP_uint16(uint16_t input, uint16_t lower_bound, uint16_t upper_bound);
long test_CLAMP_long(long input, long lower_bound, long upper_bound);
unsigned long test_CLAMP_ulong(unsigned long input, unsigned long lower_bound, unsigned long upper_bound);
float test_CLAMP_float(float input, float lower_bound, float upper_bound);

#endif //CLAMP_TEST_SHIM_H