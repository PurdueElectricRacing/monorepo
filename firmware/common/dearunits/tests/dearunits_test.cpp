/**
 * @file dearunits_test.cpp
 * @brief DearUnits runtime unit tests
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <gtest/gtest.h>

extern "C" {
#include "dearunits_test_shim.h"
}

namespace {

constexpr float METER_TOLERANCE      = 1e-6f;
constexpr float FOOT_TOLERANCE       = 1e-5f;
constexpr float CELSIUS_TOLERANCE    = 1e-5f;
constexpr float FAHRENHEIT_TOLERANCE = 2e-5f;

} // namespace

TEST(DearUnitsTest, ScaleConversions) {
    constexpr float DISTANCE_FEET   = 10.0f;
    constexpr float DISTANCE_METERS = 3.048f;

    EXPECT_NEAR(du_test_meters_from_feet(DISTANCE_FEET), DISTANCE_METERS, METER_TOLERANCE);
    EXPECT_NEAR(du_test_feet_from_meters(DISTANCE_METERS), DISTANCE_FEET, FOOT_TOLERANCE);
    EXPECT_FLOAT_EQ(du_test_meters_from_feet(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(du_test_feet_from_meters(0.0f), 0.0f);
    EXPECT_NEAR(du_test_meters_from_feet(-DISTANCE_FEET), -DISTANCE_METERS, METER_TOLERANCE);
    EXPECT_NEAR(du_test_feet_from_meters(-DISTANCE_METERS), -DISTANCE_FEET, FOOT_TOLERANCE);
}

TEST(DearUnitsTest, OffsetConversions) {
    constexpr float FREEZING_C         = 0.0f;
    constexpr float FREEZING_F         = 32.0f;
    constexpr float BOILING_C          = 100.0f;
    constexpr float BOILING_F          = 212.0f;
    constexpr float COMMON_TEMPERATURE = -40.0f;

    EXPECT_NEAR(du_test_celsius_from_fahrenheit(FREEZING_F), FREEZING_C, CELSIUS_TOLERANCE);
    EXPECT_NEAR(du_test_celsius_from_fahrenheit(BOILING_F), BOILING_C, CELSIUS_TOLERANCE);
    EXPECT_NEAR(du_test_celsius_from_fahrenheit(COMMON_TEMPERATURE),
                COMMON_TEMPERATURE,
                CELSIUS_TOLERANCE);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(FREEZING_C), FREEZING_F, FAHRENHEIT_TOLERANCE);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(BOILING_C), BOILING_F, FAHRENHEIT_TOLERANCE);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(COMMON_TEMPERATURE),
                COMMON_TEMPERATURE,
                FAHRENHEIT_TOLERANCE);
}

TEST(DearUnitsTest, SameUnitArithmetic) {
    EXPECT_FLOAT_EQ(du_test_add(-3.0f, 5.0f), 2.0f);
    EXPECT_FLOAT_EQ(du_test_subtract(3.0f, 5.0f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_negate(-3.0f), 3.0f);
    EXPECT_FLOAT_EQ(du_test_abs(-3.0f), 3.0f);
}

TEST(DearUnitsTest, DimensionalArithmetic) {
    constexpr float DISTANCE       = 12.0f;
    constexpr float TIME           = 3.0f;
    constexpr float SPEED          = 4.0f;
    constexpr float SHORT_DISTANCE = 3.0f;

    EXPECT_FLOAT_EQ(du_test_velocity(DISTANCE, TIME), SPEED);
    EXPECT_FLOAT_EQ(du_test_distance(SPEED, TIME), DISTANCE);
    EXPECT_FLOAT_EQ(du_test_ratio(SHORT_DISTANCE, DISTANCE), 0.25f);
}

TEST(DearUnitsTest, ScalarArithmetic) {
    EXPECT_FLOAT_EQ(du_test_scale(3.0f, -2.0f), -6.0f);
    EXPECT_FLOAT_EQ(du_test_scale_left(-2.0f, 3.0f), -6.0f);
    EXPECT_FLOAT_EQ(du_test_divide_scalar(6.0f, -2.0f), -3.0f);
    EXPECT_FLOAT_EQ(du_test_scale_centimeters(25.0f, 2.0f), 50.0f);
}

TEST(DearUnitsTest, CompoundConstructor) {
    EXPECT_FLOAT_EQ(du_test_force_constructor(2.0f, 12.0f, 2.0f), 6.0f);
}

TEST(DearUnitsTest, Helpers) {
    constexpr float LOWER_BOUND = -2.0f;
    constexpr float UPPER_BOUND = 2.0f;

    EXPECT_FLOAT_EQ(du_test_min(-3.0f, 2.0f), -3.0f);
    EXPECT_FLOAT_EQ(du_test_max(-3.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(-3.0f, LOWER_BOUND, UPPER_BOUND), LOWER_BOUND);
    EXPECT_FLOAT_EQ(du_test_clamp(0.0f, LOWER_BOUND, UPPER_BOUND), 0.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(3.0f, LOWER_BOUND, UPPER_BOUND), UPPER_BOUND);
    EXPECT_FLOAT_EQ(du_test_clamp(LOWER_BOUND, LOWER_BOUND, UPPER_BOUND), LOWER_BOUND);
    EXPECT_FLOAT_EQ(du_test_clamp(UPPER_BOUND, LOWER_BOUND, UPPER_BOUND), UPPER_BOUND);
    EXPECT_TRUE(du_test_lt(1.0f, 2.0f));
    EXPECT_TRUE(du_test_gt(2.0f, 1.0f));
    EXPECT_FALSE(du_test_lt(2.0f, 2.0f));
    EXPECT_FALSE(du_test_gt(2.0f, 2.0f));
    EXPECT_TRUE(du_test_le(2.0f, 2.0f));
    EXPECT_TRUE(du_test_ge(2.0f, 2.0f));
    EXPECT_FALSE(du_test_le(2.0f, 1.0f));
    EXPECT_FALSE(du_test_ge(1.0f, 2.0f));
    EXPECT_TRUE(du_test_nearly_equal(1.0f, 1.25f, 0.25f));
    EXPECT_FALSE(du_test_nearly_equal(1.0f, 1.5f, 0.25f));
    EXPECT_TRUE(du_test_nearly_equal(1.0f, 1.0f, 0.0f));
}

TEST(DearUnitsTest, SquareAndRoot) {
    EXPECT_FLOAT_EQ(du_test_square(-3.0f), 9.0f);
    EXPECT_FLOAT_EQ(du_test_sqrt(9.0f), 3.0f);
    EXPECT_FLOAT_EQ(du_test_square(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(du_test_sqrt(0.0f), 0.0f);
}

TEST(DearUnitsTest, Reciprocals) {
    constexpr float PERIOD    = 0.25f;
    constexpr float FREQUENCY = 4.0f;

    EXPECT_FLOAT_EQ(du_test_frequency(PERIOD), FREQUENCY);
    EXPECT_FLOAT_EQ(du_test_period(FREQUENCY), PERIOD);
}
