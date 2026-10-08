/**
 * @file valid_operations.c
 * @brief Test that valid unit operations compile
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/dearunits/generated/dearunits.h"

void test(void) {
    meter_t distance = DU_METER_FROM(((centimeter_t){100.0f}));
    meter_t sum = DU_ADD(distance, distance);
    meters_per_second_t velocity = DU_DIVIDE(distance, ((second_t){2.0f}));
    meter_t product = DU_MULTIPLY(velocity, ((second_t){2.0f}));
    centimeter_t scaled = DU_MULTIPLY(((centimeter_t){1.0f}), 2.0f);
    float ratio = DU_DIVIDE(distance, distance);
    bool ordered = DU_LT(distance, sum);
    (void)product;
    (void)scaled;
    (void)ratio;
    (void)ordered;
}
