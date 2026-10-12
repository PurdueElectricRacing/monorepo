/**
 * @file max_test.cpp
 * @brief Maximum Value Function Unit Tests
 * 
 * @author Kerem Eyce (keyce@purdue.edu)
 */

#include <gtest/gtest.h>
#include <cstdint>

extern "C" {
#include "max_test_shim.h"
}

// Direct inline function tests
TEST(MaxTest, SignedInt) {
    EXPECT_EQ(test_max2_signed(10, 20), 20);
    EXPECT_EQ(test_max2_signed(-10, -5), -5);
    EXPECT_EQ(test_max2_signed(0, -1), 0);
    EXPECT_EQ(test_max2_signed(100, 100), 100);
}

TEST(MaxTest, UnsignedInt) {
    EXPECT_EQ(test_max2_unsigned(10u, 20u), 20u);
    EXPECT_EQ(test_max2_unsigned(0u, 5u), 5u);
    EXPECT_EQ(test_max2_unsigned(3000000000u, 100u), 3000000000u);
}

TEST(MaxTest, Float) {
    EXPECT_FLOAT_EQ(test_max2_float(1.5f, 2.7f), 2.7f);
    EXPECT_FLOAT_EQ(test_max2_float(-1.5f, -0.5f), -0.5f);
    EXPECT_FLOAT_EQ(test_max2_float(0.0f, -0.0f), 0.0f);
}

// MAXOF2 & promotion tests
TEST(MaxTest, MaxOf2Types) {
    int a = -5, b = 15;
    EXPECT_EQ(test_MAXOF2_int(a, b), 15);

    unsigned int c = 50u, d = 25u;
    EXPECT_EQ(test_MAXOF2_uint(c, d), 50u);

    float e = 10.5f, f = 20.5f;
    EXPECT_FLOAT_EQ(test_MAXOF2_float(e, f), 20.5f);
}

TEST(MaxTest, PromotionBehavior) {

    int8_t x = -10, y = 5;
    auto result_signed = test_MAXOF2_int(x, y);
    EXPECT_EQ(result_signed, 5);
    static_assert(std::is_same_v<decltype(result_signed), signed int>, "Typecast (typeof(a)) preserves target type");

    uint16_t u1 = 100, u2 = 500;
    auto result_unsigned = test_MAXOF2_uint(u1, u2);
    EXPECT_EQ(result_unsigned, 500);
    static_assert(std::is_same_v<decltype(result_unsigned), unsigned int>, "Typecast (typeof(a)) preserves target type");
}


// MAXOF3 and MAXOF4 tests
TEST(MaxTest, MaxOf3Arguments) {
    EXPECT_EQ(test_MAXOF3_int(10, 5, 2), 10);
    EXPECT_EQ(test_MAXOF3_int(5, 10, 2), 10);
    EXPECT_EQ(test_MAXOF3_int(2, 5, 10), 10);

    EXPECT_FLOAT_EQ(test_MAXOF3_float(1.1f, 3.3f, 2.2f), 3.3f);
}

TEST(MaxTest, MaxOf4Arguments) {
    EXPECT_EQ(test_MAXOF4_int(100, 20, 10, 5), 100);
    EXPECT_EQ(test_MAXOF4_int(20, 100, 10, 5), 100);
    EXPECT_EQ(test_MAXOF4_int(20, 10, 100, 5), 100);
    EXPECT_EQ(test_MAXOF4_int(20, 10, 5, 100), 100);

    EXPECT_EQ(test_MAXOF4_int(-10, -50, -5, -20), -5);
}
