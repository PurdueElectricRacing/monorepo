#include <gtest/gtest.h>

extern "C" {
#include "rescale_test_shim.h"
}

TEST(RescaleTest, floatValues) {
    EXPECT_FLOAT_EQ(test_rescale_float(10.f, 5.f, 5.f, 6.f, 8.f), 6.f);

    EXPECT_FLOAT_EQ(test_rescale_float(6.f, 5.f, 10.f, 2.f, 7.f), 3.f);
}

TEST(RescaleTest, signedValues) {
    EXPECT_EQ(test_rescale_signed(6, 5, 10, 2, 7), 3);
}

TEST(RescaleTest, unsignedValues) {
    EXPECT_EQ(test_rescale_unsigned(16, 15, 25, 5, 17), 6);
}