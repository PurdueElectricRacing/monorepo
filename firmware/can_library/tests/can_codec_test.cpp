#include <gtest/gtest.h>

#include <algorithm>
#include <climits>
#include <cstdint>

extern "C" {
#include "can_codec_test_shim.h"
}

TEST(CanCodec, PayloadLoadStore) {
    uint8_t bytes[8];
    std::fill(std::begin(bytes), std::end(bytes), 0xAA);
    test_CAN_store_payload_u64(bytes, 0x1122334455667788ULL, 0);
    for (uint8_t byte : bytes) {
        EXPECT_EQ(byte, 0xAA);
    }

    test_CAN_store_payload_u64(bytes, 0x1122334455667788ULL, 3);
    EXPECT_EQ(bytes[0], 0x88);
    EXPECT_EQ(bytes[1], 0x77);
    EXPECT_EQ(bytes[2], 0x66);
    for (int i = 3; i < 8; ++i) {
        EXPECT_EQ(bytes[i], 0xAA);
    }
    EXPECT_EQ(test_CAN_load_payload_u64(bytes, 3), 0x667788ULL);

    test_CAN_store_payload_u64(bytes, 0x1122334455667788ULL, 8);
    EXPECT_EQ(test_CAN_load_payload_u64(bytes, 8), 0x1122334455667788ULL);
    std::fill(std::begin(bytes), std::end(bytes), 0xAA);
    EXPECT_EQ(test_CAN_load_payload_u64(bytes, 0), 0ULL);
}

TEST(CanCodec, ByteSwap) {
    EXPECT_EQ(test_CAN_apply_bswap(0x123456789ABCDEF0ULL, 0), 0x123456789ABCDEF0ULL);
    EXPECT_EQ(test_CAN_apply_bswap(0x1234ULL, 16), 0x3412ULL);
    EXPECT_EQ(test_CAN_apply_bswap(0x12345678ULL, 32), 0x78563412ULL);
    EXPECT_EQ(test_CAN_apply_bswap(0x0123456789ABCDEFULL, 64), 0xEFCDAB8967452301ULL);
    EXPECT_EQ(test_CAN_apply_bswap(0x123456789ABCDEF0ULL, 7),
              0x123456789ABCDEF0ULL);
}

TEST(CanCodec, RawSignalPackUnpack) {
    uint64_t payload = test_CAN_pack_raw_signal(0, 0xAB, 0xFF, 0, 0);
    EXPECT_EQ(payload, 0xABULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0xFF, 0, 0), 0xABULL);
    payload = test_CAN_pack_raw_signal(0, 0x5, 0xF, 12, 0);
    EXPECT_EQ(payload, 0x5000ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0xF, 12, 0), 0x5ULL);
    payload = test_CAN_pack_raw_signal(payload, 0xFFFF, 0x3, 0, 0);
    EXPECT_EQ(payload & 0x3, 0x3ULL);
    EXPECT_EQ(payload & ~0x3ULL, 0x5000ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0x3, 0, 0), 0x3ULL);
    payload = test_CAN_pack_raw_signal(0xF000000000000000ULL, 0x12, 0xFF, 8, 0);
    EXPECT_EQ(payload & 0xF000000000000000ULL, 0xF000000000000000ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0xFF, 8, 0), 0x12ULL);

    payload = test_CAN_pack_raw_signal(0, 0x1234, 0xFFFF, 0, 16);
    EXPECT_EQ(payload, 0x3412ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0xFFFF, 0, 16), 0x1234ULL);
    payload = test_CAN_pack_raw_signal(0, 0x12345678, 0xFFFFFFFFULL, 8, 32);
    EXPECT_EQ(payload, 0x7856341200ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, 0xFFFFFFFFULL, 8, 32), 0x12345678ULL);
    payload = test_CAN_pack_raw_signal(0, 0x0123456789ABCDEFULL, UINT64_MAX, 0, 64);
    EXPECT_EQ(payload, 0xEFCDAB8967452301ULL);
    EXPECT_EQ(test_CAN_unpack_raw_signal(payload, UINT64_MAX, 0, 64), 0x0123456789ABCDEFULL);
}

TEST(CanCodec, SignExtension) {
    EXPECT_EQ(test_CAN_sign_extend_raw(0, 0), 0);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x7F, 8), 127);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x80, 8), -128);
    EXPECT_EQ(test_CAN_sign_extend_raw(0xFF, 8), -1);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x7FF, 12), 2047);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x800, 12), -2048);
    EXPECT_EQ(test_CAN_sign_extend_raw(0xFFF, 12), -1);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x7FFF, 16), 32767);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x8000, 16), -32768);
    EXPECT_EQ(test_CAN_sign_extend_raw(0xFFFF, 16), -1);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x7FFFFFFFFFFFFFFFULL, 64), INT64_MAX);
    EXPECT_EQ(test_CAN_sign_extend_raw(0x8000000000000000ULL, 64), INT64_MIN);
}

TEST(CanCodec, FloatBitRoundTrips) {
    EXPECT_EQ(test_CAN_float32_to_raw(1.0f), 0x3F800000ULL);
    uint64_t raw = test_CAN_float32_to_raw(-12.5f);
    EXPECT_EQ(test_CAN_float32_to_raw(test_CAN_raw_to_float32(raw)), raw);
    EXPECT_EQ(test_CAN_float32_to_raw(test_CAN_raw_to_float32(0x40490FDBULL)), 0x40490FDBULL);
}
