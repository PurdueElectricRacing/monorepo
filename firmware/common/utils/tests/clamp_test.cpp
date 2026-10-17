#include <gtest/gtest.h>

extern "C" {
#include "clamp_test_shim.h"
}

TEST(ClampTest, ClampsInt) {
    EXPECT_EQ(test_CLAMP_int(-4, 1, 3), 1);
    EXPECT_EQ(test_CLAMP_int(7, 1, 3), 3);
    EXPECT_EQ(test_CLAMP_int(2, 1, 3), 2);
    EXPECT_EQ(test_CLAMP_int(-5, -3, -1), -3);
    EXPECT_EQ(test_CLAMP_int(5, -3, -1), -1);
    EXPECT_EQ(test_CLAMP_int(-2, -3, -1), -2);
    EXPECT_EQ(test_CLAMP_int(1, 1, 2), 1);
    EXPECT_EQ(test_CLAMP_int(2, 1, 2), 2);
}

TEST(ClampTest, ClampsInt8) {
    EXPECT_EQ(test_CLAMP_int8((int8_t) -4, (int8_t) 1, (int8_t) 3), (int8_t) 1);
    EXPECT_EQ(test_CLAMP_int8((int8_t) 7, (int8_t) 1, (int8_t) 3), (int8_t) 3);
    EXPECT_EQ(test_CLAMP_int8((int8_t) 2, (int8_t) 1, (int8_t) 3), (int8_t) 2);
    EXPECT_EQ(test_CLAMP_int8((int8_t) -5, (int8_t) -3, (int8_t) -1), (int8_t) -3);
    EXPECT_EQ(test_CLAMP_int8((int8_t) 5, (int8_t) -3, (int8_t) -1), (int8_t) -1);
    EXPECT_EQ(test_CLAMP_int8((int8_t) -2, (int8_t) -3, (int8_t) -1), (int8_t) -2);
    EXPECT_EQ(test_CLAMP_int8((int8_t) 1, (int8_t) 1, (int8_t) 2), (int8_t) 1);
    EXPECT_EQ(test_CLAMP_int8((int8_t) 2, (int8_t) 1, (int8_t) 2), (int8_t) 2);
}

TEST(ClampTest, ClampsInt16) {
    EXPECT_EQ(test_CLAMP_int16((int16_t) -4, (int16_t) 1, (int16_t) 3), (int16_t) 1);
    EXPECT_EQ(test_CLAMP_int16((int16_t) 7, (int16_t) 1, (int16_t) 3), (int16_t) 3);
    EXPECT_EQ(test_CLAMP_int16((int16_t) 2, (int16_t) 1, (int16_t) 3), (int16_t) 2);
    EXPECT_EQ(test_CLAMP_int16((int16_t) -5, (int16_t) -3, (int16_t) -1), (int16_t) -3);
    EXPECT_EQ(test_CLAMP_int16((int16_t) 5, (int16_t) -3, (int16_t) -1), (int16_t) -1);
    EXPECT_EQ(test_CLAMP_int16((int16_t) -2, (int16_t) -3, (int16_t) -1), (int16_t) -2);
    EXPECT_EQ(test_CLAMP_int16((int16_t) 1, (int16_t) 1, (int16_t) 2), (int16_t) 1);
    EXPECT_EQ(test_CLAMP_int16((int16_t) 2, (int16_t) 1, (int16_t) 2), (int16_t) 2);
}

TEST(ClampTest, ClampsUnsignedInt) {
    EXPECT_EQ(test_CLAMP_uint((unsigned int) 4, (unsigned int) 7, (unsigned int) 10), (unsigned int) 7);
    EXPECT_EQ(test_CLAMP_uint((unsigned int) 14, (unsigned int) 7, (unsigned int) 10), (unsigned int) 10);
    EXPECT_EQ(test_CLAMP_uint((unsigned int) 8, (unsigned int) 7, (unsigned int) 10), (unsigned int) 8);
    EXPECT_EQ(test_CLAMP_uint((unsigned int) 1, (unsigned int) 1, (unsigned int) 2), (unsigned int) 1);
    EXPECT_EQ(test_CLAMP_uint((unsigned int) 2, (unsigned int) 1, (unsigned int) 2), (unsigned int) 2);
}

TEST(ClampTest, ClampsUnsignedInt8) {
    EXPECT_EQ(test_CLAMP_uint8((uint8_t) 4, (uint8_t) 7, (uint8_t) 10), (uint8_t) 7);
    EXPECT_EQ(test_CLAMP_uint8((uint8_t) 14, (uint8_t) 7, (uint8_t) 10), (uint8_t) 10);
    EXPECT_EQ(test_CLAMP_uint8((uint8_t) 8, (uint8_t) 7, (uint8_t) 10), (uint8_t) 8);
    EXPECT_EQ(test_CLAMP_uint8((uint8_t) 1, (uint8_t) 1, (uint8_t) 2), (uint8_t) 1);
    EXPECT_EQ(test_CLAMP_uint8((uint8_t) 2, (uint8_t) 1, (uint8_t) 2), (uint8_t) 2);
}

TEST(ClampTest, ClampsUnsignedInt16) {
    EXPECT_EQ(test_CLAMP_uint16((uint16_t) 4, (uint16_t) 7, (uint16_t) 10), (uint16_t) 7);
    EXPECT_EQ(test_CLAMP_uint16((uint16_t) 14, (uint16_t) 7, (uint16_t) 10), (uint16_t) 10);
    EXPECT_EQ(test_CLAMP_uint16((uint16_t) 8, (uint16_t) 7, (uint16_t) 10), (uint16_t) 8);
    EXPECT_EQ(test_CLAMP_uint16((uint16_t) 1, (uint16_t) 1, (uint16_t) 2), (uint16_t) 1);
    EXPECT_EQ(test_CLAMP_uint16((uint16_t) 2, (uint16_t) 1, (uint16_t) 2), (uint16_t) 2);
}

TEST(ClampTest, ClampsLong) {
    EXPECT_EQ(test_CLAMP_long((long) -4, (long) 1, (long) 3), (long) 1);
    EXPECT_EQ(test_CLAMP_long((long) 7, (long) 1, (long) 3), (long) 3);
    EXPECT_EQ(test_CLAMP_long((long) 2, (long) 1, (long) 3), (long) 2);
    EXPECT_EQ(test_CLAMP_long((long) -5, (long) -3, (long) -1), (long) -3);
    EXPECT_EQ(test_CLAMP_long((long) 5, (long) -3, (long) -1), (long) -1);
    EXPECT_EQ(test_CLAMP_long((long) -2, (long) -3, (long) -1), (long) -2);
    EXPECT_EQ(test_CLAMP_long((long) 1, (long) 1, (long) 2), (long) 1);
    EXPECT_EQ(test_CLAMP_long((long) 2, (long) 1, (long) 2), (long) 2);
}

TEST(ClampTest, ClampsUnsignedLong) {
    EXPECT_EQ(test_CLAMP_ulong((unsigned long) 4, (unsigned long) 7, (unsigned long) 10), (unsigned long) 7);
    EXPECT_EQ(test_CLAMP_ulong((unsigned long) 14, (unsigned long) 7, (unsigned long) 10), (unsigned long) 10);
    EXPECT_EQ(test_CLAMP_ulong((unsigned long) 4, (unsigned long) 8, (unsigned long) 10), (unsigned long) 8);
    EXPECT_EQ(test_CLAMP_ulong((unsigned long) 1, (unsigned long) 1, (unsigned long) 2), (unsigned long) 1);
    EXPECT_EQ(test_CLAMP_ulong((unsigned long) 2, (unsigned long) 1, (unsigned long) 2), (unsigned long) 2);
}

TEST(ClampTest, ClampsFloat) {
    EXPECT_FLOAT_EQ(test_CLAMP_float(-4.0, 1.0, 3.0), 1.0);
    EXPECT_FLOAT_EQ(test_CLAMP_float(7.0, 1.0, 3.0), 3.0);
    EXPECT_FLOAT_EQ(test_CLAMP_float(2.0, 1.0, 3.0), 2.0);
    EXPECT_FLOAT_EQ(test_CLAMP_float(-5.0, -3.0, -1.0), -3.0);
    EXPECT_FLOAT_EQ(test_CLAMP_float(5.0, -3.0, -1), -1);
    EXPECT_FLOAT_EQ(test_CLAMP_float(-2.0, -3.0, -1), -2);
    EXPECT_FLOAT_EQ(test_CLAMP_float(1.0, 1.0, 2.0), 1.0);
    EXPECT_FLOAT_EQ(test_CLAMP_float(2.0, 1.0, 2.0), 2.0);
}