#include "rescale_test_shim.h"
#include "rescale.h"

float test_rescale_float(float input, float input_min, float input_max, float output_min, float output_max) {
    return rescale_float(input, input_min, input_max, output_min, output_max);
}

signed int test_rescale_signed(signed int input, signed int input_min, signed int input_max, signed int output_min, signed int output_max) {
    return rescale_signed(input, input_min, input_max, output_min, output_max);
}

unsigned int test_rescale_unsigned(unsigned int input, unsigned int input_min, unsigned int input_max, unsigned int output_min, unsigned int output_max) {
    return rescale_unsigned(input, input_min, input_max, output_min, output_max);
}

signed int test_rescale_generic_signed(signed int input, signed int input_min, signed int input_max, signed int output_min, signed int output_max) {
    return RESCALE(input, input_min, input_max, output_min, output_max);
}

unsigned int test_rescale_generic_unsigned(unsigned int input, unsigned int input_min, unsigned int input_max, unsigned int output_min, unsigned int output_max) {
    return RESCALE(input, input_min, input_max, output_min, output_max);
}

float test_rescale_generic_float(float input, float input_min, float input_max, float output_min, float output_max) {
    return RESCALE(input, input_min, input_max, output_min, output_max);
}

float test_rescale_generic_float_diffargs(float input, float input_min, unsigned int input_max, unsigned int output_min, signed int output_max) {
    return RESCALE(input, input_min, input_max, output_min, output_max);
}
