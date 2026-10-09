/**
 * @file reject_incompatible_comparison.c
 * @brief Test that incompatible units cannot be compared
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    meter_t distance = (meter_t) {.value = 1.0f};
    second_t time    = (second_t) {.value = 1.0f};

    (void)DU_LT(distance, time);
}
