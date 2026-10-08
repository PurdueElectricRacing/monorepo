/**
 * @file reject_unsupported_product.c
 * @brief Test that unsupported dimensional products are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    (void)DU_MULTIPLY(((meter_t){1.0f}), ((amp_t){1.0f}));
}
