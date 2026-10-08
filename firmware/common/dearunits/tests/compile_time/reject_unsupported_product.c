/**
 * @file reject_unsupported_product.c
 * @brief Test that unsupported dimensional products are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    meter_t distance = (meter_t) {.value = 1.0f};
    amp_t current    = (amp_t) {.value = 1.0f};

    (void)DU_MULTIPLY(distance, current);
}
