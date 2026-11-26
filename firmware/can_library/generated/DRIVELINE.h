#ifndef DRIVELINE_H
#define DRIVELINE_H

/**
 * @file DRIVELINE.h
 * @brief Auto-generated CAN driver for DRIVELINE.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include <string.h>

#include "can_library/can_codec.h"
#include "can_library/can_common.h"
#include "can_library/generated/can_types.h"
#include "can_library/generated/can_version.h"
#include "can_library/generated/SCAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_DRIVELINE uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN1

void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg);

// Scaling Macros
// front_oil_temps
static constexpr float PACK_COEFF_FRONT_OIL_TEMPS_LEFT = 100.0f;
static constexpr float PACK_COEFF_FRONT_OIL_TEMPS_RIGHT = 100.0f;
// front_shockpots
static constexpr float PACK_COEFF_FRONT_SHOCKPOTS_LEFT = 100.0f;
static constexpr float PACK_COEFF_FRONT_SHOCKPOTS_RIGHT = 100.0f;
// rear_oil_temps
static constexpr float PACK_COEFF_REAR_OIL_TEMPS_LEFT = 100.0f;
static constexpr float PACK_COEFF_REAR_OIL_TEMPS_RIGHT = 100.0f;
// rear_shockpots
static constexpr float PACK_COEFF_REAR_SHOCKPOTS_LEFT = 100.0f;
static constexpr float PACK_COEFF_REAR_SHOCKPOTS_RIGHT = 100.0f;

// Offset Macros

// Data Structures
typedef struct {
    bl_front_driveline_start_data_t bl_front_driveline_start;
    bl_rear_driveline_start_data_t bl_rear_driveline_start;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void bl_front_driveline_start_CALLBACK();
extern void bl_rear_driveline_start_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN1) {
        switch (id) {
            case BL_FRONT_DRIVELINE_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_front_driveline_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_front_driveline_start.last_rx = xTaskGetTickCount();
                bl_front_driveline_start_CALLBACK();
                break;
            }
            case BL_REAR_DRIVELINE_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_rear_driveline_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_rear_driveline_start.last_rx = xTaskGetTickCount();
                bl_rear_driveline_start_CALLBACK();
                break;
            }
            default:
                break;
        }
    }
}

// Stale Checking

// Data initialization
static inline void CAN_init_data(void) {
    memset((void *)&can_data, 0, sizeof(can_data));
    // Underflow the last_rx timers to ensure messages are stale until first rx
    // Setup the stale function pointers
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_front_driveline_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_DRIVELINE_INIT_MSG_ID,
        .IDE = false,
        .DLC = FRONT_DRIVELINE_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_DRIVELINE_INIT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_driveline_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_DRIVELINE_INIT_MSG_ID,
        .IDE = false,
        .DLC = REAR_DRIVELINE_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_DRIVELINE_INIT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_shockpots(
    uint16_t value_left,
    uint16_t value_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_SHOCKPOTS_MSG_ID,
        .IDE = false,
        .DLC = FRONT_SHOCKPOTS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_left = (uint64_t)value_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_left, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_right = (uint64_t)value_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_right, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_SHOCKPOTS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_shockpots(
    uint16_t value_left,
    uint16_t value_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_SHOCKPOTS_MSG_ID,
        .IDE = false,
        .DLC = REAR_SHOCKPOTS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_left = (uint64_t)value_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_left, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_right = (uint64_t)value_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_right, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_SHOCKPOTS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_oil_temps(
    int16_t value_left,
    int16_t value_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_OIL_TEMPS_MSG_ID,
        .IDE = false,
        .DLC = FRONT_OIL_TEMPS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_left = (uint64_t)value_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_left, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_right = (uint64_t)value_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_right, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_OIL_TEMPS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_oil_temps(
    int16_t value_left,
    int16_t value_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_OIL_TEMPS_MSG_ID,
        .IDE = false,
        .DLC = REAR_OIL_TEMPS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_left = (uint64_t)value_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_left, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_right = (uint64_t)value_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_right, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_OIL_TEMPS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_driveline_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_DRIVELINE_VERSION_MSG_ID,
        .IDE = false,
        .DLC = FRONT_DRIVELINE_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_DRIVELINE_VERSION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_driveline_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_DRIVELINE_VERSION_MSG_ID,
        .IDE = false,
        .DLC = REAR_DRIVELINE_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_DRIVELINE_VERSION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_driveline_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_DRIVELINE_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = FRONT_DRIVELINE_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_DRIVELINE_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_driveline_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_DRIVELINE_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = FRONT_DRIVELINE_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_DRIVELINE_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_front_driveline_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = FRONT_DRIVELINE_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = FRONT_DRIVELINE_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, FRONT_DRIVELINE_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_driveline_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_DRIVELINE_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = REAR_DRIVELINE_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_DRIVELINE_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_driveline_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_DRIVELINE_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = REAR_DRIVELINE_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_DRIVELINE_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_rear_driveline_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = REAR_DRIVELINE_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = REAR_DRIVELINE_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, REAR_DRIVELINE_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN1, SCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN1
static inline void FDCAN1_init_filters() {
    static const uint32_t sid_list[] = {
        BL_FRONT_DRIVELINE_START_MSG_ID,
        BL_REAR_DRIVELINE_START_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN1,
        sid_list, 2,
        NULL, 0
    );
}

#endif // DRIVELINE_H
