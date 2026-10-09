/**
 * @file valid_operations.c
 * @brief Test that valid unit operations compile
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    centimeter_t length       = (centimeter_t) {.value = 100.0f};
    second_t time             = (second_t) {.value = 2.0f};
    centimeter_t small_length = (centimeter_t) {.value = 1.0f};

    meter_t distance             = DU_METER_FROM(length);
    meter_t sum                  = DU_ADD(distance, distance);
    meters_per_second_t velocity = DU_DIVIDE(distance, time);
    meter_t product              = DU_MULTIPLY(velocity, time);
    centimeter_t scaled          = DU_MULTIPLY(small_length, 2.0f);
    float ratio                  = DU_DIVIDE(distance, distance);
    bool ordered                 = DU_LT(distance, sum);

    // discard unused warnings
    (void)product;
    (void)scaled;
    (void)ratio;
    (void)ordered;
}
