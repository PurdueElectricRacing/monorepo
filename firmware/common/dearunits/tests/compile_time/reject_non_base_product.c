/**
 * @file reject_non_base_product.c
 * @brief Test that dimensional products require base units
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    centimeter_t length = (centimeter_t) {.value = 1.0f};
    meter_t width       = (meter_t) {.value = 1.0f};

    (void)DU_MULTIPLY(length, width);
}
