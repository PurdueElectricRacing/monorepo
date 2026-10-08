#include "common/dearunits/generated/dearunits.h"

void test(void) {
    second_t result = DU_MULTIPLY(((meters_per_second_t){2.0f}), ((second_t){3.0f}));
    (void)result;
}
