#include "abs_test_shim.h"

// GCC reports the draft C2x value while compiling its C23 implementation.

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202000L
#error "abs tests must compile this shim as C23 or newer"
#endif

#include "abs.h"

float test_ABS_f(float value) {
    return ABS(value);
}

int test_ABS_i(int value) {
    return ABS(value);
}