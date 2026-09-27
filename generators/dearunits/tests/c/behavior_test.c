#include <math.h>
#include <stdio.h>

#include "dear_units.h"

static int checks;
static int failures;

#define CHECK(condition)                                            \
    do {                                                            \
        checks++;                                                   \
        if (!(condition)) {                                         \
            failures++;                                             \
            printf("FAIL line %d: %s\n", __LINE__, #condition);     \
        }                                                           \
    } while (0)

static bool near(float actual, float expected) {
    float scale = fabsf(expected) > 1.0f ? fabsf(expected) : 1.0f;
    return fabsf(actual - expected) <= 1e-5f * scale;
}

static int evaluations;

static meter_t counted(float value) {
    evaluations++;
    return (meter_t){ .value = value };
}

static void test_unit_ops(void) {
    meter_t two = { .value = 2.0f };
    meter_t five = { .value = 5.0f };
    meter_t minus_three = { .value = -3.0f };
    meter_t zero = { .value = 0.0f };

    CHECK(DEARUNITS_ADD(two, five).value == 7.0f);
    CHECK(DEARUNITS_SUBTRACT(five, two).value == 3.0f);
    CHECK(DEARUNITS_NEGATE(two).value == -2.0f);
    CHECK(DEARUNITS_ABS(minus_three).value == 3.0f);
    CHECK(DEARUNITS_SIGN(minus_three) == -1.0f);
    CHECK(DEARUNITS_SIGN(five) == 1.0f);
    CHECK(DEARUNITS_SIGN(zero) == 0.0f);
    CHECK(DEARUNITS_COPYSIGN(two, minus_three).value == -2.0f);
    CHECK(DEARUNITS_COPYSIGN(minus_three, five).value == 3.0f);
    CHECK(DEARUNITS_MIN(two, five).value == 2.0f);
    CHECK(DEARUNITS_MAX(two, five).value == 5.0f);
    CHECK(DEARUNITS_MIN(five, two).value == 2.0f);
    CHECK(DEARUNITS_MAX(five, two).value == 5.0f);
    CHECK(DEARUNITS_CLAMP(counted(3.0f), two, five).value == 3.0f);
    CHECK(DEARUNITS_CLAMP(minus_three, two, five).value == 2.0f);
    CHECK(DEARUNITS_CLAMP((meter_t){ .value = 9.0f }, two, five).value == 5.0f);
    CHECK(DEARUNITS_SATURATE((meter_t){ .value = 9.0f }, two).value == 2.0f);
    CHECK(DEARUNITS_SATURATE((meter_t){ .value = -9.0f }, two).value == -2.0f);
    CHECK(DEARUNITS_SATURATE((meter_t){ .value = 1.0f }, two).value == 1.0f);
    CHECK(DEARUNITS_SATURATE((meter_t){ .value = 9.0f }, minus_three).value == 3.0f);
    CHECK(DEARUNITS_DEADBAND((meter_t){ .value = 0.5f }, two).value == 0.0f);
    CHECK(DEARUNITS_DEADBAND((meter_t){ .value = 2.5f }, two).value == 2.5f);
    CHECK(DEARUNITS_DEADBAND((meter_t){ .value = 2.0f }, two).value == 2.0f);
    CHECK(DEARUNITS_SLEW(zero, five, two).value == 2.0f);
    CHECK(DEARUNITS_SLEW((meter_t){ .value = 4.0f }, five, two).value == 5.0f);
    CHECK(DEARUNITS_SLEW((meter_t){ .value = 9.0f }, five, two).value == 7.0f);
    CHECK(DEARUNITS_SLEW(zero, five, zero).value == 0.0f);
    CHECK(DEARUNITS_SLEW(zero, five, minus_three).value == 3.0f);
    CHECK(DEARUNITS_ABS_DIFF(two, five).value == 3.0f);
    CHECK(DEARUNITS_ABS_DIFF(five, two).value == 3.0f);
    CHECK(DEARUNITS_LERP(two, five, 0.5f).value == 3.5f);
    CHECK(DEARUNITS_LERP(two, five, 0.0f).value == 2.0f);
    CHECK(DEARUNITS_LERP(two, five, 1.0f).value == 5.0f);
    CHECK(near(DEARUNITS_INVLERP(two, five, counted(3.0f)), 1.0f / 3.0f));
    CHECK(DEARUNITS_HYPOT((meter_t){ .value = 3.0f }, (meter_t){ .value = 4.0f }).value == 5.0f);
    CHECK(DEARUNITS_FMOD(five, two).value == 1.0f);
    CHECK(DEARUNITS_FMOD(minus_three, two).value == -1.0f);
    CHECK(DEARUNITS_ROUND((meter_t){ .value = 2.5f }).value == 3.0f);
    CHECK(DEARUNITS_FLOOR((meter_t){ .value = 2.7f }).value == 2.0f);
    CHECK(DEARUNITS_CEIL((meter_t){ .value = 2.1f }).value == 3.0f);
    CHECK(DEARUNITS_NEARLY_EQUAL(two, (meter_t){ .value = 2.05f }, (meter_t){ .value = 0.1f }));
    CHECK(!DEARUNITS_NEARLY_EQUAL(two, five, (meter_t){ .value = 0.1f }));
    CHECK(DEARUNITS_LT(two, five) && !DEARUNITS_LT(five, two) && !DEARUNITS_LT(two, two));
    CHECK(DEARUNITS_GT(five, two) && !DEARUNITS_GT(two, five) && !DEARUNITS_GT(two, two));
    CHECK(DEARUNITS_LE(two, five) && DEARUNITS_LE(two, two) && !DEARUNITS_LE(five, two));
    CHECK(DEARUNITS_GE(five, two) && DEARUNITS_GE(two, two) && !DEARUNITS_GE(two, five));
    CHECK(DEARUNITS_IS_FINITE(two) && !DEARUNITS_IS_FINITE((meter_t){ .value = INFINITY }));
    CHECK(!DEARUNITS_IS_NAN(two) && !DEARUNITS_IS_NAN((meter_t){ .value = INFINITY }));

    meter_t mapped = DEARUNITS_MAP_RANGE((second_t){ .value = 5.0f }, (second_t){ .value = 0.0f },
                                         (second_t){ .value = 10.0f }, two, five);
    CHECK(mapped.value == 3.5f);
    CHECK(evaluations == 2);

    celsius_t hot = { .value = 90.0f };
    CHECK(DEARUNITS_MAX(hot, (celsius_t){ .value = 20.0f }).value == 90.0f);
}

static void test_nan_policy(void) {
    float nan = __builtin_nanf("");
    meter_t bad = { .value = nan };
    meter_t good = { .value = 2.0f };
    meter_t low = { .value = 0.0f };
    meter_t high = { .value = 5.0f };
    meter_t step = { .value = 1.0f };

    CHECK(isnan(DEARUNITS_ADD(bad, good).value) && isnan(DEARUNITS_ADD(good, bad).value));
    CHECK(isnan(DEARUNITS_SUBTRACT(bad, good).value) && isnan(DEARUNITS_SUBTRACT(good, bad).value));
    CHECK(isnan(DEARUNITS_NEGATE(bad).value));
    CHECK(isnan(DEARUNITS_ABS(bad).value));
    CHECK(isnan(DEARUNITS_SIGN(bad)));
    CHECK(isnan(DEARUNITS_COPYSIGN(bad, good).value) && isnan(DEARUNITS_COPYSIGN(good, bad).value));
    CHECK(isnan(DEARUNITS_MIN(bad, good).value) && isnan(DEARUNITS_MIN(good, bad).value));
    CHECK(isnan(DEARUNITS_MAX(bad, good).value) && isnan(DEARUNITS_MAX(good, bad).value));
    CHECK(isnan(DEARUNITS_CLAMP(bad, low, high).value));
    CHECK(isnan(DEARUNITS_CLAMP(good, bad, high).value));
    CHECK(isnan(DEARUNITS_CLAMP(good, low, bad).value));
    CHECK(isnan(DEARUNITS_SATURATE(bad, good).value) && isnan(DEARUNITS_SATURATE(good, bad).value));
    CHECK(isnan(DEARUNITS_DEADBAND(bad, good).value) && isnan(DEARUNITS_DEADBAND(good, bad).value));
    CHECK(isnan(DEARUNITS_SLEW(bad, high, step).value));
    CHECK(isnan(DEARUNITS_SLEW(low, bad, step).value));
    CHECK(isnan(DEARUNITS_SLEW(low, high, bad).value));
    CHECK(isnan(DEARUNITS_ABS_DIFF(bad, good).value) && isnan(DEARUNITS_ABS_DIFF(good, bad).value));
    CHECK(isnan(DEARUNITS_LERP(bad, high, 0.5f).value));
    CHECK(isnan(DEARUNITS_LERP(low, bad, 0.5f).value));
    CHECK(isnan(DEARUNITS_LERP(low, high, nan).value));
    CHECK(isnan(DEARUNITS_INVLERP(bad, high, good)) && isnan(DEARUNITS_INVLERP(low, high, bad)));
    CHECK(isnan(DEARUNITS_HYPOT(bad, good).value) && isnan(DEARUNITS_HYPOT(good, bad).value));
    CHECK(isnan(DEARUNITS_FMOD(bad, good).value) && isnan(DEARUNITS_FMOD(good, bad).value));
    CHECK(isnan(DEARUNITS_ROUND(bad).value) && isnan(DEARUNITS_FLOOR(bad).value) && isnan(DEARUNITS_CEIL(bad).value));
    CHECK(isnan(DEARUNITS_MAP_RANGE(bad, low, high, low, high).value));
    CHECK(isnan(DEARUNITS_MULTIPLY(bad, 2.0f).value) && isnan(DEARUNITS_MULTIPLY(2.0f, bad).value));
    CHECK(isnan(DEARUNITS_DIVIDE(bad, 2.0f).value));
    CHECK(isnan(DEARUNITS_DIVIDE(bad, good)) && isnan(DEARUNITS_DIVIDE(good, bad)));
    CHECK(isnan(DEARUNITS_SIN((radian_t){ .value = nan })) && isnan(DEARUNITS_COS((degree_t){ .value = nan })));
    CHECK(isnan(DEARUNITS_WRAP_ANGLE((degree_t){ .value = nan }).value));
    CHECK(isnan(DEARUNITS_WRAP_ANGLE_POSITIVE((degree_t){ .value = nan }).value));
    CHECK(isnan(DEARUNITS_ANGLE_DIFF((degree_t){ .value = nan }, (degree_t){ .value = 1.0f }).value));
    CHECK(isnan(DEARUNITS_ATAN2(bad, good).value) && isnan(DEARUNITS_ATAN2(good, bad).value));

    CHECK(!DEARUNITS_LT(bad, good) && !DEARUNITS_LT(good, bad));
    CHECK(!DEARUNITS_GT(bad, good) && !DEARUNITS_GT(good, bad));
    CHECK(!DEARUNITS_LE(bad, good) && !DEARUNITS_LE(good, bad));
    CHECK(!DEARUNITS_GE(bad, good) && !DEARUNITS_GE(good, bad));
    CHECK(!DEARUNITS_NEARLY_EQUAL(bad, bad, step) && !DEARUNITS_NEARLY_EQUAL(good, good, bad));
    CHECK(DEARUNITS_IS_NAN(bad) && !DEARUNITS_IS_FINITE(bad));
}

static void test_angles(void) {
    radian_t half_turn = { .value = 3.14159265f };
    degree_t right_angle = { .value = 90.0f };
    revolution_t quarter_and_one = { .value = 1.25f };

    CHECK(near(DEARUNITS_SIN(right_angle), 1.0f));
    CHECK(fabsf(DEARUNITS_COS(right_angle)) < 1e-6f);
    CHECK(near(DEARUNITS_TAN((degree_t){ .value = 45.0f }), 1.0f));
    CHECK(fabsf(DEARUNITS_SIN(half_turn)) < 1e-6f);
    CHECK(near(DEARUNITS_SIN(quarter_and_one), 1.0f));
    CHECK(near(DEARUNITS_WRAP_ANGLE((degree_t){ .value = 190.0f }).value, -170.0f));
    CHECK(near(DEARUNITS_WRAP_ANGLE((degree_t){ .value = -190.0f }).value, 170.0f));
    CHECK(near(DEARUNITS_WRAP_ANGLE((degree_t){ .value = 180.0f }).value, -180.0f));
    CHECK(near(DEARUNITS_WRAP_ANGLE((revolution_t){ .value = 0.75f }).value, -0.25f));
    CHECK(near(DEARUNITS_WRAP_ANGLE((radian_t){ .value = 4.0f }).value, 4.0f - 6.2831853f));
    CHECK(near(DEARUNITS_WRAP_ANGLE_POSITIVE((degree_t){ .value = -90.0f }).value, 270.0f));
    CHECK(near(DEARUNITS_WRAP_ANGLE_POSITIVE((degree_t){ .value = 370.0f }).value, 10.0f));
    CHECK(near(DEARUNITS_ANGLE_DIFF((degree_t){ .value = 10.0f }, (degree_t){ .value = 350.0f }).value, 20.0f));
    CHECK(near(DEARUNITS_ANGLE_DIFF((degree_t){ .value = 350.0f }, (degree_t){ .value = 10.0f }).value, -20.0f));
    CHECK(near(DEARUNITS_ASIN(1.0f).value, 1.5707963f));
    CHECK(near(DEARUNITS_ACOS(0.0f).value, 1.5707963f));
    CHECK(near(DEARUNITS_ATAN(1.0f).value, 0.7853982f));
    CHECK(near(DEARUNITS_ATAN2((meter_t){ .value = 1.0f }, (meter_t){ .value = 1.0f }).value, 0.7853982f));
    CHECK(near(DEARUNITS_ATAN2((meters_per_second_t){ .value = 0.0f }, (meters_per_second_t){ .value = -1.0f }).value, 3.1415927f));
    CHECK(near(degree_from_radian(DEARUNITS_ATAN(1.0f)).value, 45.0f));

    for (int i = -20000; i < 20000; i++) {
        float angle = (float)i * 7.3f;
        float signed_wrap = DEARUNITS_WRAP_ANGLE((degree_t){ .value = angle }).value;
        float positive_wrap = DEARUNITS_WRAP_ANGLE_POSITIVE((degree_t){ .value = angle }).value;
        CHECK(signed_wrap >= -180.0f && signed_wrap < 180.0f);
        CHECK(positive_wrap >= 0.0f && positive_wrap < 360.0f);
    }
}

static void test_pairs(void) {
    meter_t five_meters = { .value = 5.0f };
    second_t four_seconds = { .value = 4.0f };
    meters_per_second_t speed = DEARUNITS_DIVIDE(five_meters, four_seconds);
    CHECK(speed.value == 1.25f);
    CHECK(DEARUNITS_MULTIPLY(speed, four_seconds).value == 5.0f);
    CHECK(DEARUNITS_MULTIPLY(four_seconds, speed).value == 5.0f);
    CHECK(DEARUNITS_DIVIDE(five_meters, speed).value == 4.0f);
    CHECK(DEARUNITS_DIVIDE(five_meters, (meter_t){ .value = 2.0f }) == 2.5f);

    meters_per_second_squared_t acceleration = DEARUNITS_DIVIDE(speed, four_seconds);
    CHECK(acceleration.value == 0.3125f);
    newton_t force = DEARUNITS_MULTIPLY((kilogram_t){ .value = 10.0f }, acceleration);
    CHECK(force.value == 3.125f);
    CHECK(DEARUNITS_MULTIPLY(force, (meter_t){ .value = 2.0f }).value == 6.25f);
    CHECK(DEARUNITS_MULTIPLY((meter_t){ .value = 2.0f }, force).value == 6.25f);
    CHECK(DEARUNITS_DIVIDE(DEARUNITS_MULTIPLY(force, (meter_t){ .value = 2.0f }), (meter_t){ .value = 2.0f }).value == 3.125f);
    CHECK(DEARUNITS_DIVIDE(DEARUNITS_MULTIPLY(force, (meter_t){ .value = 2.0f }), force).value == 2.0f);
    CHECK(DEARUNITS_MULTIPLY((newton_t){ .value = 10.0f }, (meters_per_second_t){ .value = 3.0f }).value == 30.0f);
    CHECK(DEARUNITS_MULTIPLY((radians_per_second_t){ .value = 3.0f }, four_seconds).value == 12.0f);
    CHECK(DEARUNITS_DIVIDE((radian_t){ .value = 12.0f }, four_seconds).value == 3.0f);

    square_meter_t area = DEARUNITS_SQUARE(five_meters);
    CHECK(area.value == 25.0f && DEARUNITS_SQRT(area).value == 5.0f);
    CHECK(DEARUNITS_MULTIPLY(five_meters, (meter_t){ .value = 2.0f }).value == 10.0f);
    CHECK(DEARUNITS_DIVIDE(area, five_meters).value == 5.0f);
    CHECK(DEARUNITS_MULTIPLY((pascal_t){ .value = 100.0f }, area).value == 2500.0f);
    CHECK(DEARUNITS_DIVIDE((newton_t){ .value = 2500.0f }, (pascal_t){ .value = 100.0f }).value == 25.0f);

    volt_t volts = { .value = 400.0f };
    amp_t amps = { .value = 50.0f };
    watt_t power = DEARUNITS_MULTIPLY(volts, amps);
    CHECK(power.value == 20000.0f && DEARUNITS_MULTIPLY(amps, volts).value == 20000.0f);
    CHECK(DEARUNITS_DIVIDE(power, volts).value == 50.0f);
    CHECK(DEARUNITS_DIVIDE(power, amps).value == 400.0f);
    CHECK(DEARUNITS_DIVIDE(volts, amps).value == 8.0f);
    CHECK(DEARUNITS_MULTIPLY(amps, (ohm_t){ .value = 8.0f }).value == 400.0f);

    joule_t energy = DEARUNITS_MULTIPLY(power, (second_t){ .value = 3600.0f });
    CHECK(energy.value == 72e6f);
    CHECK(near(kilowatt_hour_from_joule(energy).value, 20.0f));
    CHECK(DEARUNITS_DIVIDE(energy, (second_t){ .value = 3600.0f }).value == 20000.0f);
    CHECK(DEARUNITS_DIVIDE(energy, power).value == 3600.0f);
    coulomb_t charge = DEARUNITS_MULTIPLY(amps, (second_t){ .value = 7200.0f });
    CHECK(near(amp_hour_from_coulomb(charge).value, 100.0f));
    CHECK(DEARUNITS_MULTIPLY(charge, volts).value == 360000.0f * 400.0f);
    CHECK(DEARUNITS_DIVIDE(DEARUNITS_MULTIPLY(charge, volts), volts).value == 360000.0f);

    newton_meter_t torque = { .value = 40.0f };
    radians_per_second_t speed_rad = { .value = 100.0f };
    watt_t mechanical = DEARUNITS_MULTIPLY(torque, speed_rad);
    CHECK(mechanical.value == 4000.0f);
    CHECK(DEARUNITS_DIVIDE(mechanical, speed_rad).value == 40.0f);
    CHECK(DEARUNITS_DIVIDE(mechanical, torque).value == 100.0f);
    CHECK(DEARUNITS_MULTIPLY((newton_t){ .value = 10.0f }, (meter_t){ .value = 2.0f }).value == 20.0f);
    CHECK(DEARUNITS_DIVIDE(torque, torque) == 1.0f);
    CHECK(DEARUNITS_DIVIDE(energy, energy) == 1.0f);

    hertz_t rate = DEARUNITS_DIVIDE(1.0f, (second_t){ .value = 0.5f });
    CHECK(rate.value == 2.0f);
    CHECK(DEARUNITS_DIVIDE(1.0f, rate).value == 0.5f);
    CHECK(DEARUNITS_MULTIPLY(rate, (second_t){ .value = 3.0f }) == 6.0f);
    CHECK(DEARUNITS_DIVIDE(power, rate).value == 10000.0f);
    CHECK(DEARUNITS_MULTIPLY(energy, rate).value == 144e6f);
}

static void test_scalars(void) {
    meter_t five = { .value = 5.0f };
    mile_t four_miles = { .value = 4.0f };
    CHECK(DEARUNITS_MULTIPLY(five, 2.0f).value == 10.0f);
    CHECK(DEARUNITS_MULTIPLY(2.0f, five).value == 10.0f);
    CHECK(DEARUNITS_DIVIDE(five, 2.0f).value == 2.5f);
    CHECK(DEARUNITS_MULTIPLY(four_miles, 0.5f).value == 2.0f);
    CHECK(DEARUNITS_MULTIPLY(0.5f, four_miles).value == 2.0f);
    CHECK(DEARUNITS_DIVIDE(four_miles, 2.0f).value == 2.0f);
    CHECK(DEARUNITS_MULTIPLY(DEARUNITS_MULTIPLY(0.5f, (meters_per_second_t){ .value = 20.0f }), (second_t){ .value = 4.0f }).value == 40.0f);
}

static void test_constructors(void) {
    meter_t length = { .value = 3.0f };
    second_t duration = { .value = 2.0f };
    kilogram_t mass = { .value = 5.0f };
    amp_t current = { .value = 4.0f };
    newton_t force = { .value = 10.0f };
    volt_t voltage = { .value = 12.0f };
    radian_t angle = { .value = 6.0f };
    watt_t power = { .value = 60.0f };

    CHECK(near(velocity_from(length, duration).value, 1.5f));
    CHECK(near(acceleration_from(length, duration).value, 0.75f));
    CHECK(near(angular_velocity_from(angle, duration).value, 3.0f));
    CHECK(near(force_from(mass, length, duration).value, 3.75f));
    CHECK(near(torque_from(force, length).value, 30.0f));
    CHECK(near(pressure_from(force, length).value, 10.0f / 9.0f));
    CHECK(near(voltage_from(mass, length, duration, current).value, 45.0f / 32.0f));
    CHECK(near(area_from(length).value, 9.0f));
    CHECK(near(frequency_from(duration).value, 0.5f));
    CHECK(near(charge_from(current, duration).value, 8.0f));
    CHECK(near(resistance_from(voltage, current).value, 3.0f));
    CHECK(near(power_from(force, length, duration).value, 15.0f));
    CHECK(near(energy_from(power, duration).value, 120.0f));
}

#define CHECK_DISPATCH(macro, unit_type, expected) \
    CHECK(near(macro((unit_type){ .value = 1.0f }).value, (expected)))

static void test_dispatch_macros(void) {
    CHECK_DISPATCH(DEARUNITS_METER_FROM, kilometer_t, 1000.0f);
    CHECK_DISPATCH(DEARUNITS_METER_FROM, mile_t, 1609.344f);
    CHECK_DISPATCH(DEARUNITS_SECOND_FROM, minute_t, 60.0f);
    CHECK_DISPATCH(DEARUNITS_KILOGRAM_FROM, gram_t, 0.001f);
    CHECK_DISPATCH(DEARUNITS_AMP_FROM, milliamp_t, 0.001f);
    CHECK_DISPATCH(DEARUNITS_RADIAN_FROM, revolution_t, 6.2831853f);
    CHECK_DISPATCH(DEARUNITS_METERS_PER_SECOND_FROM, miles_per_hour_t, 0.44704f);
    CHECK_DISPATCH(DEARUNITS_METERS_PER_SECOND_SQUARED_FROM, standard_gravity_t, 9.80665f);
    CHECK_DISPATCH(DEARUNITS_RADIANS_PER_SECOND_FROM, revolutions_per_minute_t, 0.10471976f);
    CHECK_DISPATCH(DEARUNITS_NEWTON_FROM, pound_force_t, 4.4482216f);
    CHECK_DISPATCH(DEARUNITS_NEWTON_METER_FROM, pound_foot_t, 1.3558179f);
    CHECK_DISPATCH(DEARUNITS_PASCAL_FROM, bar_t, 100000.0f);
    CHECK_DISPATCH(DEARUNITS_VOLT_FROM, millivolt_t, 0.001f);
    CHECK_DISPATCH(DEARUNITS_SQUARE_METER_FROM, square_inch_t, 0.00064516f);
    CHECK_DISPATCH(DEARUNITS_HERTZ_FROM, kilohertz_t, 1000.0f);
    CHECK_DISPATCH(DEARUNITS_COULOMB_FROM, amp_hour_t, 3600.0f);
    CHECK_DISPATCH(DEARUNITS_OHM_FROM, kiloohm_t, 1000.0f);
    CHECK_DISPATCH(DEARUNITS_WATT_FROM, horsepower_t, 745.69987f);
    CHECK_DISPATCH(DEARUNITS_JOULE_FROM, kilowatt_hour_t, 3600000.0f);
    CHECK_DISPATCH(DEARUNITS_CELSIUS_FROM, kelvin_t, -272.15f);
}

int main(void) {
    test_unit_ops();
    test_nan_policy();
    test_angles();
    test_pairs();
    test_scalars();
    test_constructors();
    test_dispatch_macros();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
