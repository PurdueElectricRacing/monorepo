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
    uint32_t identity; // [3 bit bus ID @31-29] [29 bits CAN ID @28-0]
    uint64_t payload;  // message data
} timestamped_frame_t;

static_assert(
    sizeof(timestamped_frame_t) == 16,
    "timestamped_frame_t should be 16 bytes for optimal access patterns"
);

// helper functions
[[gnu::always_inline]]
static inline void set_bus_id(timestamped_frame_t *frame, uint32_t bus_id) {
    static constexpr uint32_t BUS_ID_MASK = (0b111u << 29u); // 3 bits for bus ID

    uint32_t shifted_id = bus_id << 29u;
    frame->identity &= ~BUS_ID_MASK; // Clear existing bus ID bits
    frame->identity |= (shifted_id & BUS_ID_MASK); // Set new bus ID bits
}

[[gnu::always_inline]]
static inline void set_can_id(timestamped_frame_t *frame, uint32_t can_id) {
    static constexpr uint32_t CAN_ID_MASK = 0x1FFFFFFFu; // 29 bits for CAN ID

    frame->identity &= ~CAN_ID_MASK; // Clear existing CAN ID bits
    frame->identity |= (can_id & CAN_ID_MASK); // Set new CAN ID bits
}

#endif // TIMESTAMPED_FRAME_H