/**
 * @file reject_wrong_result_type.c
 * @brief Test that incorrect result types are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    second_t result = DU_MULTIPLY(((meters_per_second_t){2.0f}), ((second_t){3.0f}));
    (void)result;
}
