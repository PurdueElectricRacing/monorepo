#include <gtest/gtest.h>

extern "C" {
#include "rescale_test_shim.h"
}

TEST(RescaleTest, FloatValues) {
    EXPECT_FLOAT_EQ(test_rescale_float(10.f, 5.f, 5.f, 6.f, 8.f), 6.f);

    EXPECT_FLOAT_EQ(test_rescale_float(6.f, 5.f, 10.f, 2.f, 7.f), 3.f);
}

TEST(RescaleTest, SignedValues) {
    EXPECT_EQ(test_rescale_signed(6, 5, 10, 2, 7), 3);
}

TEST(RescaleTest, UnsignedValues) {
    EXPECT_EQ(test_rescale_unsigned(16, 15, 25, 5, 17), 6);
}

TEST(RescaleTest, GenericSigned) {
    EXPECT_EQ(test_rescale_generic_signed(7, 0, 10, 0, 100), 70);
}

TEST(RescaleTest, GenericFloat) {
    EXPECT_FLOAT_EQ(test_rescale_generic_float(2.5f, 0.0f, 5.0f, 0.0f, 1.0f), 0.5f);
}

TEST(RescaleTest, GenericUnsigned) {
    EXPECT_EQ(test_rescale_generic_unsigned(50u, 0u, 100u, 0u, 200u), 100u);
}

TEST(RescaleTest, GenericDifferentValues) {
    EXPECT_FLOAT_EQ(test_rescale_generic_float_diffargs(50.f, 5.f, 55u, 5u, 10), 9.5f);
}
