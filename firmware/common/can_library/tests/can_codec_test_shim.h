#ifndef CAN_CODEC_TEST_SHIM_H
#define CAN_CODEC_TEST_SHIM_H

#include <stdint.h>

uint64_t test_CAN_apply_bswap(uint64_t raw, uint8_t bswap_width);
uint64_t test_CAN_load_payload_u64(const uint8_t *data, uint8_t len);
void test_CAN_store_payload_u64(uint8_t *dest, uint64_t payload, uint8_t len);
uint64_t test_CAN_pack_raw_signal(
    uint64_t payload,
    uint64_t raw,
    uint64_t mask,
    uint8_t bit_shift,
    uint8_t bswap_width
);
uint64_t test_CAN_unpack_raw_signal(
    uint64_t payload,
    uint64_t mask,
    uint8_t bit_shift,
    uint8_t bswap_width
);
int64_t test_CAN_sign_extend_raw(uint64_t raw, uint8_t bit_length);
uint64_t test_CAN_float32_to_raw(float value);
float test_CAN_raw_to_float32(uint64_t raw);

#endif  // CAN_CODEC_TEST_SHIM_H
