/**
 * @file reject_incompatible_comparison.c
 * @brief Test that incompatible units cannot be compared
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    (void)DU_LT(((meter_t){1.0f}), ((second_t){1.0f}));
}
