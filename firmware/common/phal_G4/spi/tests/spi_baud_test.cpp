#include <gtest/gtest.h>

extern "C" {
#include "spi_baud.h"
}

namespace {

constexpr uint32_t k16MHz  = 16'000'000U;
constexpr uint32_t k170MHz = 170'000'000U;

// Marker that a failed selection must leave untouched
constexpr uint32_t kUnwritten = 0xFFFF'FFFFU;

}  // namespace

TEST(SpiBaud, ExactPowerOfTwoRatioSelectsThatDivider) {
    uint32_t br = kUnwritten;
    ASSERT_TRUE(PHAL_SPI_selectBaudRatePrescaler(k16MHz, 1'000'000U, &br));
    EXPECT_EQ(br, 3U);  // 16 MHz / 16 = 1 MHz
}

TEST(SpiBaud, NonPowerOfTwoRatioRoundsToSlowerClock) {
    uint32_t br = kUnwritten;
    ASSERT_TRUE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, 1'000'000U, &br));
    EXPECT_EQ(br, 7U);  // /128 would be 1.33 MHz (too fast), so /256 = 664 kHz
}

TEST(SpiBaud, SelectedClockNeverExceedsRequestAtDividerBoundary) {
    // 170 MHz / 256 = 664062.5 Hz, which is just above 664062 Hz
    uint32_t br = kUnwritten;
    EXPECT_FALSE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, 664'062U, &br));
    EXPECT_EQ(br, kUnwritten);
    ASSERT_TRUE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, 664'063U, &br));
    EXPECT_EQ(br, 7U);
}

TEST(SpiBaud, RequestAtOrAboveHalfBusClockSelectsFastestDivider) {
    uint32_t br = kUnwritten;
    ASSERT_TRUE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, k170MHz / 2U, &br));
    EXPECT_EQ(br, 0U);
    ASSERT_TRUE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, k170MHz, &br));
    EXPECT_EQ(br, 0U);
}

TEST(SpiBaud, RequestSlowerThanSlowestDividerFails) {
    uint32_t br = kUnwritten;
    EXPECT_FALSE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, 500'000U, &br));
    EXPECT_EQ(br, kUnwritten);
}

TEST(SpiBaud, ZeroRequestFails) {
    uint32_t br = kUnwritten;
    EXPECT_FALSE(PHAL_SPI_selectBaudRatePrescaler(k170MHz, 0U, &br));
    EXPECT_EQ(br, kUnwritten);
}
