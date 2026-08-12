#include <gtest/gtest.h>

extern "C" {
#include "lerp_lut.h"
}

namespace {

constexpr lut_entry_t kEntries[] = {
    {0.0F, 0.0F},
    {1.0F, 10.0F},
    {2.0F, 20.0F},
    {4.0F, 40.0F},
};

const lerp_lut_t kLut = {kEntries, sizeof(kEntries) / sizeof(kEntries[0])};

}  // namespace

TEST(LerpLut, ReturnsExactLookupPoints) {
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 0.0F), 0.0F);
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 1.0F), 10.0F);
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 2.0F), 20.0F);
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 4.0F), 40.0F);
}

TEST(LerpLut, InterpolatesBetweenLookupPoints) {
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 0.5F), 5.0F);
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 3.0F), 30.0F);
}

TEST(LerpLut, ClampsBelowLowerBound) {
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, -1.0F), 0.0F);
}

TEST(LerpLut, ClampsAboveUpperBound) {
    EXPECT_FLOAT_EQ(lut_lookup(&kLut, 5.0F), 40.0F);
}
