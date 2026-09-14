#ifndef TIMESTAMPED_FRAME_H
#define TIMESTAMPED_FRAME_H

/**
 * @file timestamped_frame.h
 * @brief Definition for timestamped CAN frames.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

typedef struct {
    uint32_t ticks_ms; // ms timestamp of reception
    uint32_t identity; // [2 bits bus ID] [1 bit isExtID] [29 bits CAN ID]
    uint64_t payload;  // message data
} timestamped_frame_t;

static_assert(
    sizeof(timestamped_frame_t) == 16,
    "timestamped_frame_t should be 16 bytes for optimal access patterns"
);

// helper functions
[[gnu::always_inline]]
static inline void set_bus_id(timestamped_frame_t *frame, uint8_t bus_id) {
    static constexpr uint32_t BUS_ID_MASK = 0xC0000000u; // [31:30]
    static constexpr uint32_t BUS_ID_SHIFT = 30u;

    frame->identity &= ~BUS_ID_MASK;
    frame->identity |= (uint32_t)(bus_id & 0x3u) << BUS_ID_SHIFT;
}

[[gnu::always_inline]]
static inline void set_xid(timestamped_frame_t *frame, bool is_xid) {
    static constexpr uint32_t IS_XID_MASK = (1u << 29u); // [29]

    if (is_xid) {
        frame->identity |= IS_XID_MASK;
    } else {
        frame->identity &= ~IS_XID_MASK;
    }
}

[[gnu::always_inline]]
static inline void set_can_id(timestamped_frame_t *frame, uint32_t can_id) {
    static constexpr uint32_t CAN_ID_MASK = 0x1FFFFFFFu; // 29 bits for CAN ID [28:0]

    frame->identity &= ~CAN_ID_MASK; // Clear existing CAN ID bits
    frame->identity |= (can_id & CAN_ID_MASK); // Set new CAN ID bits
}

#endif // TIMESTAMPED_FRAME_H