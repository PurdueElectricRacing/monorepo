/**
 * @file reject_wrong_conversion.c
 * @brief Test that incompatible unit conversions are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    second_t time = (second_t) {.value = 1.0f};

    (void)DU_METER_FROM(time);
}
