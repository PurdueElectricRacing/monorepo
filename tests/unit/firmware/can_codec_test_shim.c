#include "can_codec_test_shim.h"

// GCC reports the draft C2x value while compiling its C23 implementation.
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202000L
#error "can_codec tests must compile this shim as C23 or newer"
#endif

#include "can_codec.h"

uint64_t test_CAN_apply_bswap(uint64_t raw, uint8_t bswap_width) {
    return CAN_apply_bswap(raw, (bswap_width_t)bswap_width);
}

uint64_t test_CAN_load_payload_u64(const uint8_t *data, uint8_t len) {
    return CAN_load_payload_u64(data, len);
}

void test_CAN_store_payload_u64(uint8_t *dest, uint64_t payload, uint8_t len) {
    CAN_store_payload_u64(dest, payload, len);
}

uint64_t test_CAN_pack_raw_signal(
    uint64_t payload,
    uint64_t raw,
    uint64_t mask,
    uint8_t bit_shift,
    uint8_t bswap_width
) {
    return CAN_pack_raw_signal(payload, raw, mask, bit_shift, (bswap_width_t)bswap_width);
}

uint64_t test_CAN_unpack_raw_signal(
    uint64_t payload,
    uint64_t mask,
    uint8_t bit_shift,
    uint8_t bswap_width
) {
    return CAN_unpack_raw_signal(payload, mask, bit_shift, (bswap_width_t)bswap_width);
}

int64_t test_CAN_sign_extend_raw(uint64_t raw, uint8_t bit_length) {
    return CAN_sign_extend_raw(raw, bit_length);
}

uint64_t test_CAN_float32_to_raw(float value) {
    return CAN_float32_to_raw(value);
}

float test_CAN_raw_to_float32(uint64_t raw) {
    return CAN_raw_to_float32(raw);
}
