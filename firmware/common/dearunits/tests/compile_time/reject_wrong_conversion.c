/**
 * @file reject_wrong_conversion.c
 * @brief Test that incompatible unit conversions are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    (void)DU_METER_FROM(((second_t){1.0f}));
}
