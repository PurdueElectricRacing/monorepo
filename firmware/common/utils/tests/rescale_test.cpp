#include <gtest/gtest.h>

extern "C" {
#include "rescale_test_shim.h"
}

TEST(RescaleTest, floatValues) {
    EXPECT_FLOAT_EQ(test_rescale_float(10.f, 5.f, 5.f, 6.f, 8.f), 6.f);
}