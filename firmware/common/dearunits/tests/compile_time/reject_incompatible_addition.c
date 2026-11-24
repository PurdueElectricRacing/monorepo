/**
 * @file reject_incompatible_addition.c
 * @brief Test that incompatible units cannot be added
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "dearunits/generated/dearunits.h"

void test(void) {
    meter_t distance = (meter_t) {.value = 1.0f};
    second_t time    = (second_t) {.value = 1.0f};

    (void)DU_ADD(distance, time);
}
