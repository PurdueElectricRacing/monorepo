#include "dearunits_test_shim.h"

#include "common/dearunits/generated/dearunits.h"

// Keep C23 dispatch in C; inputs and assertions live in Google Test.

float du_test_meters_from_feet(float x) {
    foot_t operand_0 = {.value = x};

    meter_t result = DU_METER_FROM(operand_0);
    return result.value;
}

float du_test_feet_from_meters(float x) {
    meter_t operand_0 = {.value = x};

    return foot_from_meter(operand_0).value;
}

float du_test_celsius_from_fahrenheit(float x) {
    fahrenheit_t operand_0 = {.value = x};

    return DU_CELSIUS_FROM(operand_0).value;
}

float du_test_fahrenheit_from_celsius(float x) {
    celsius_t operand_0 = {.value = x};

    return fahrenheit_from_celsius(operand_0).value;
}

float du_test_velocity(float a, float b) {
    meter_t operand_0  = {.value = a};
    second_t operand_1 = {.value = b};

    meters_per_second_t result = DU_DIVIDE(operand_0, operand_1);
    return result.value;
}

float du_test_distance(float a, float b) {
    meters_per_second_t operand_0 = {.value = a};
    second_t operand_1            = {.value = b};

    meter_t result = DU_MULTIPLY(operand_0, operand_1);
    return result.value;
}

float du_test_ratio(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    float result = DU_DIVIDE(operand_0, operand_1);
    return result;
}

float du_test_scale(float a, float b) {
    meter_t operand_0 = {.value = a};

    return DU_MULTIPLY(operand_0, b).value;
}

float du_test_scale_left(float a, float b) {
    meter_t operand_0 = {.value = b};

    return DU_MULTIPLY(a, operand_0).value;
}

float du_test_scale_centimeters(float a, float b) {
    centimeter_t operand_0 = {.value = a};

    return DU_MULTIPLY(operand_0, b).value;
}

float du_test_divide_scalar(float a, float b) {
    meter_t operand_0 = {.value = a};

    return DU_DIVIDE(operand_0, b).value;
}

float du_test_frequency(float x) {
    second_t operand_0 = {.value = x};

    hertz_t result = DU_DIVIDE(1.0f, operand_0);
    return result.value;
}

float du_test_period(float x) {
    hertz_t operand_0 = {.value = x};

    second_t result = DU_DIVIDE(1.0f, operand_0);
    return result.value;
}

float du_test_square(float x) {
    meter_t operand_0 = {.value = x};

    square_meter_t result = DU_SQUARE(operand_0);
    return result.value;
}

float du_test_sqrt(float x) {
    square_meter_t operand_0 = {.value = x};

    meter_t result = DU_SQRT(operand_0);
    return result.value;
}

float du_test_force_constructor(float mass, float length, float time) {
    kilogram_t operand_0 = {.value = mass};
    meter_t operand_1    = {.value = length};
    second_t operand_2   = {.value = time};

    newton_t result = force_from(operand_0, operand_1, operand_2);
    return result.value;
}

float du_test_add(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_ADD(operand_0, operand_1).value;
}

float du_test_subtract(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_SUBTRACT(operand_0, operand_1).value;
}

float du_test_min(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_MIN(operand_0, operand_1).value;
}

float du_test_max(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_MAX(operand_0, operand_1).value;
}

float du_test_negate(float x) {
    meter_t operand_0 = {.value = x};

    return DU_NEGATE(operand_0).value;
}

float du_test_abs(float x) {
    meter_t operand_0 = {.value = x};

    return DU_ABS(operand_0).value;
}

float du_test_clamp(float x, float lo, float hi) {
    meter_t operand_0 = {.value = x};
    meter_t operand_1 = {.value = lo};
    meter_t operand_2 = {.value = hi};

    return DU_CLAMP(operand_0, operand_1, operand_2).value;
}

bool du_test_lt(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_LT(operand_0, operand_1);
}

bool du_test_gt(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_GT(operand_0, operand_1);
}

bool du_test_le(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_LE(operand_0, operand_1);
}

bool du_test_ge(float a, float b) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};

    return DU_GE(operand_0, operand_1);
}

bool du_test_nearly_equal(float a, float b, float tol) {
    meter_t operand_0 = {.value = a};
    meter_t operand_1 = {.value = b};
    meter_t operand_2 = {.value = tol};

    return DU_NEARLY_EQUAL(operand_0, operand_1, operand_2);
}
