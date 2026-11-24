/**
 * @file can_codec.h
 * @brief Force-inlined helpers for generated CAN signal packing and unpacking.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef CAN_CODEC_H
#define CAN_CODEC_H

#include <stdint.h>
#include <string.h>

typedef enum : uint8_t {
    BSWAP_NONE = 0,
    BSWAP_16   = 16,
    BSWAP_32   = 32,
    BSWAP_64   = 64
} bswap_width_t;

[[gnu::always_inline]]
static inline uint64_t CAN_apply_bswap(uint64_t raw, bswap_width_t bswap_width) {
    switch (bswap_width) {
        case BSWAP_16:
            return __builtin_bswap16((uint16_t)raw);
        case BSWAP_32:
            return __builtin_bswap32((uint32_t)raw);
        case BSWAP_64:
            return __builtin_bswap64(raw);
        default:
            return raw;
    }
}

[[gnu::always_inline]]
static inline uint64_t CAN_load_payload_u64(const uint8_t *data, uint8_t len) {
    uint64_t payload = 0;
    memcpy(&payload, data, len);
    return payload;
}

[[gnu::always_inline]]
static inline void CAN_store_payload_u64(uint8_t *dest, uint64_t payload, uint8_t len) {
    memcpy(dest, &payload, len);
}

[[gnu::always_inline]]
static inline uint64_t CAN_pack_raw_signal(
    uint64_t payload,
    uint64_t raw,
    uint64_t mask,
    uint8_t bit_shift,
    bswap_width_t bswap_width
) {
    raw &= mask;
    raw = CAN_apply_bswap(raw, bswap_width);
    payload |= (raw & mask) << bit_shift;
    return payload;
}

[[gnu::always_inline]]
static inline uint64_t CAN_unpack_raw_signal(
    uint64_t payload,
    uint64_t mask,
    uint8_t bit_shift,
    bswap_width_t bswap_width
) {
    uint64_t raw = (payload >> bit_shift) & mask;
    return CAN_apply_bswap(raw, bswap_width);
}

[[gnu::always_inline]]
static inline int64_t CAN_sign_extend_raw(uint64_t raw, uint8_t bit_length) {
    if (bit_length == 0) {
        return 0;
    }
    if (bit_length >= 64) {
        return (int64_t)raw;
    }

    uint8_t shift = 64U - bit_length;
    return (int64_t)(raw << shift) >> shift;
}

[[gnu::always_inline]]
static inline uint64_t CAN_float32_to_raw(float value) {
    uint32_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

[[gnu::always_inline]]
static inline float CAN_raw_to_float32(uint64_t raw) {
    uint32_t bits = (uint32_t)raw;
    float value   = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#endif // CAN_CODEC_H
