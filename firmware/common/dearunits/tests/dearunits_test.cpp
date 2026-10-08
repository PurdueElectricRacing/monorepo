#include <gtest/gtest.h>

extern "C" {
#include "dearunits_test_shim.h"
}

TEST(DearUnitsTest, ScaleConversions) {
    EXPECT_NEAR(du_test_meters_from_feet(10.0f), 3.048f, 1e-6f);
    EXPECT_NEAR(du_test_feet_from_meters(3.048f), 10.0f, 1e-5f);
    EXPECT_FLOAT_EQ(du_test_meters_from_feet(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(du_test_feet_from_meters(0.0f), 0.0f);
    EXPECT_NEAR(du_test_meters_from_feet(-10.0f), -3.048f, 1e-6f);
    EXPECT_NEAR(du_test_feet_from_meters(-3.048f), -10.0f, 1e-5f);
}

TEST(DearUnitsTest, OffsetConversions) {
    EXPECT_NEAR(du_test_celsius_from_fahrenheit(32.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(du_test_celsius_from_fahrenheit(212.0f), 100.0f, 1e-5f);
    EXPECT_NEAR(du_test_celsius_from_fahrenheit(-40.0f), -40.0f, 1e-5f);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(0.0f), 32.0f, 1e-5f);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(100.0f), 212.0f, 2e-5f);
    EXPECT_NEAR(du_test_fahrenheit_from_celsius(-40.0f), -40.0f, 1e-5f);
}

TEST(DearUnitsTest, SameUnitArithmetic) {
    EXPECT_FLOAT_EQ(du_test_add(-3.0f, 5.0f), 2.0f);
    EXPECT_FLOAT_EQ(du_test_subtract(3.0f, 5.0f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_negate(-3.0f), 3.0f);
    EXPECT_FLOAT_EQ(du_test_abs(-3.0f), 3.0f);
    EXPECT_FLOAT_EQ(du_test_abs_diff(3.0f, 5.0f), 2.0f);
}

TEST(DearUnitsTest, DimensionalArithmetic) {
    EXPECT_FLOAT_EQ(du_test_velocity(12.0f, 3.0f), 4.0f);
    EXPECT_FLOAT_EQ(du_test_distance(4.0f, 3.0f), 12.0f);
    EXPECT_FLOAT_EQ(du_test_ratio(3.0f, 12.0f), 0.25f);
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
    EXPECT_FLOAT_EQ(du_test_min(-3.0f, 2.0f), -3.0f);
    EXPECT_FLOAT_EQ(du_test_max(-3.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(-3.0f, -2.0f, 2.0f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(0.0f, -2.0f, 2.0f), 0.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(3.0f, -2.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(-2.0f, -2.0f, 2.0f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_clamp(2.0f, -2.0f, 2.0f), 2.0f);
    EXPECT_TRUE(du_test_lt(1.0f, 2.0f));
    EXPECT_TRUE(du_test_gt(2.0f, 1.0f));
    EXPECT_FALSE(du_test_lt(2.0f, 2.0f));
    EXPECT_FALSE(du_test_gt(2.0f, 2.0f));
    EXPECT_TRUE(du_test_le(2.0f, 2.0f));
    EXPECT_TRUE(du_test_ge(2.0f, 2.0f));
    EXPECT_FALSE(du_test_le(2.0f, 1.0f));
    EXPECT_FALSE(du_test_ge(1.0f, 2.0f));
    EXPECT_FLOAT_EQ(du_test_round(-1.5f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_floor(-1.25f), -2.0f);
    EXPECT_FLOAT_EQ(du_test_ceil(-1.25f), -1.0f);
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
    EXPECT_FLOAT_EQ(du_test_frequency(0.25f), 4.0f);
    EXPECT_FLOAT_EQ(du_test_period(4.0f), 0.25f);
}
