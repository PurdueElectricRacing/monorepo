#include <gtest/gtest.h>

extern "C" {
#include "abs_test_shim.h"
}

TEST(AbsTest, EvaluatesInts) {
    EXPECT_EQ(test_ABS_i(10), 10);
    EXPECT_EQ(test_ABS_i(-10), 10);
    EXPECT_EQ(test_ABS_i(0), 0);
}

TEST(AbsTest, EvaluatesFloats) {
    EXPECT_FLOAT_EQ(test_ABS_f(10.0F), 10.0F);
    EXPECT_FLOAT_EQ(test_ABS_f(-10.0F), 10.0F);
    EXPECT_FLOAT_EQ(test_ABS_f(0.0F), 0.0F);
}

TEST(AbsTest, IntPromotionsSuccessful) {
    EXPECT_EQ(test_ABS_i((int8_t) -10), 10);
    EXPECT_EQ(test_ABS_i((int16_t) -10), 10);
}