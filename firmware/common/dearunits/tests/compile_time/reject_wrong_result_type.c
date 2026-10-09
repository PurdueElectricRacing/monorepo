/**
 * @file reject_wrong_result_type.c
 * @brief Test that incorrect result types are rejected
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    meters_per_second_t velocity = (meters_per_second_t) {.value = 2.0f};
    second_t time                = (second_t) {.value = 3.0f};

    second_t result = DU_MULTIPLY(velocity, time);
    (void)result;
}
