/**
 * @file reject_non_base_product.c
 * @brief Test that dimensional products require base units
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    (void)DU_MULTIPLY(((centimeter_t){1.0f}), ((meter_t){1.0f}));
}
