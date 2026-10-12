/**
 * @file min_test.cpp
 * @brief Minimum Value Function Unit Tests
 * 
 * @author Kerem Eyce (keyce@purdue.edu)
 */

 #include <gtest/gtest.h>
extern "C" {
    #include "min_test_shim.h"
}
#include <cstdint>

// direct inline function tests
TEST(MinTest, SignedInt) {
    EXPECT_EQ(test_min2_signed(10, 20), 10);
    EXPECT_EQ(test_min2_signed(-10, -5), -10);
    EXPECT_EQ(test_min2_signed(0, -1), -1);
    EXPECT_EQ(test_min2_signed(100, 100), 100);
}

TEST(MinTest, UnsignedInt) {
    EXPECT_EQ(test_min2_unsigned(10u, 20u), 10u);
    EXPECT_EQ(test_min2_unsigned(0u, 5u), 0u);
    EXPECT_EQ(test_min2_unsigned(3000000000u, 20000000u), 20000000u);
}

TEST(MinTest, Float) {
    EXPECT_FLOAT_EQ(test_min2_float(1.5f, 2.7f), 1.5f);
    EXPECT_FLOAT_EQ(test_min2_float(-1.5f, -0.5f), -1.5f);
    EXPECT_FLOAT_EQ(test_min2_float(0.0f, -0.0f), 0.0f);
}

// MINOF2 & type promotion tests
TEST(MinTest, MinOf2Types) {
    int a = -5, b = 15;
    EXPECT_EQ(test_MINOF2_int(a, b), -5);
    static_assert(std::is_same_v<decltype(test_MINOF2_int(a, b)), int>, "Must return int");

    unsigned int c = 50u, d = 25u;
    EXPECT_EQ(test_MINOF2_uint(c, d), 25u);
    static_assert(std::is_same_v<decltype(test_MINOF2_uint(c, d)), unsigned int>, "Must return unsigned int");

    float e = 10.5f, f = 20.5f;
    EXPECT_FLOAT_EQ(test_MINOF2_float(e, f), 10.5f);
    static_assert(std::is_same_v<decltype(test_MINOF2_float(e, f)), float>, "Must return float");
}

TEST(MinTest, PromotionBehavior) {

    int8_t x = -10, y = 5;
    auto result_signed = test_MINOF2_int(x, y);
    EXPECT_EQ(result_signed, -10);
    static_assert(std::is_same_v<decltype(result_signed), signed int>, "Typecast (typeof(a)) preserves target type");

    uint16_t u1 = 100, u2 = 500;
    auto result_unsigned = test_MINOF2_uint(u1, u2);
    EXPECT_EQ(result_unsigned, 100);
    static_assert(std::is_same_v<decltype(result_unsigned), unsigned int>, "Typecast (typeof(a)) preserves target type");
}


// MINOF3 and MINOF4 tests
TEST(MinTest, MinOf3Arguments) {
    EXPECT_EQ(test_MINOF3_int(10, 5, 2), 2);
    EXPECT_EQ(test_MINOF3_int(5, 10, 2), 2);
    EXPECT_EQ(test_MINOF3_int(2, 5, 10), 2);

    EXPECT_FLOAT_EQ(test_MINOF3_float(1.1f, 3.3f, 2.2f), 1.1f);
}

TEST(MinTest, MinOf4Arguments) {
    EXPECT_EQ(test_MINOF4_int(100, 20, 10, 5), 5);
    EXPECT_EQ(test_MINOF4_int(20, 100, 10, 5), 5);
    EXPECT_EQ(test_MINOF4_int(20, 10, 100, 5), 5);
    EXPECT_EQ(test_MINOF4_int(20, 10, 5, 100), 5);

    EXPECT_EQ(test_MINOF4_int(-10, -50, -5, -20), -50);
}

