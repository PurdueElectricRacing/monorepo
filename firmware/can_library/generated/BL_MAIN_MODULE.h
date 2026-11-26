#ifndef BL_MAIN_MODULE_H
#define BL_MAIN_MODULE_H

/**
 * @file BL_MAIN_MODULE.h
 * @brief Auto-generated CAN driver for BL_MAIN_MODULE.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include <string.h>

#include "can_library/can_codec.h"
#include "can_library/can_common.h"
#include "can_library/generated/can_types.h"
#include "can_library/generated/can_version.h"
#include "can_library/generated/VCAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_BL_MAIN_MODULE uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN2

void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);

// Scaling Macros

// Offset Macros

// Data Structures
typedef struct {
    bl_main_module_start_data_t bl_main_module_start;
    bl_main_module_crc_data_t bl_main_module_crc;
    bl_main_module_jump_data_t bl_main_module_jump;
    bl_main_module_data_data_t bl_main_module_data;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN2) {
        switch (id) {
            case BL_MAIN_MODULE_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_main_module_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_main_module_start.last_rx = xTaskGetTickCount();
                break;
            }
            case BL_MAIN_MODULE_CRC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_data = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_main_module_crc.data = (uint32_t)sig_raw_data;

                can_data.bl_main_module_crc.last_rx = xTaskGetTickCount();
                break;
            }
            case BL_MAIN_MODULE_JUMP_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_data = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_main_module_jump.data = (uint32_t)sig_raw_data;

                can_data.bl_main_module_jump.last_rx = xTaskGetTickCount();
                break;
            }
            case BL_MAIN_MODULE_DATA_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_index = CAN_unpack_raw_signal(host_data, 0xFFFFFF, 0, BSWAP_NONE);
                can_data.bl_main_module_data.index = (uint32_t)sig_raw_index;
                uint64_t sig_raw_data = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 24, BSWAP_NONE);
                can_data.bl_main_module_data.data = (uint32_t)sig_raw_data;

                can_data.bl_main_module_data.last_rx = xTaskGetTickCount();
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
static inline void CAN_SEND_bl_main_module_resp(
    bootloader_status_t value_status,
    uint32_t value_detail
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = BL_MAIN_MODULE_RESP_MSG_ID,
        .IDE = false,
        .DLC = BL_MAIN_MODULE_RESP_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_status = (uint64_t)value_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_status, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_detail = (uint64_t)value_detail;
    payload = CAN_pack_raw_signal(payload, sig_raw_detail, 0xFFFFFFFF, 8, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BL_MAIN_MODULE_RESP_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bl_main_module_info(
    uint8_t value_protocol_version,
    uint32_t value_git_hash,
    bootloader_target_id_t value_target_id,
    uint8_t value_flags
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = BL_MAIN_MODULE_INFO_MSG_ID,
        .IDE = false,
        .DLC = BL_MAIN_MODULE_INFO_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_protocol_version = (uint64_t)value_protocol_version;
    payload = CAN_pack_raw_signal(payload, sig_raw_protocol_version, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFFF, 8, BSWAP_NONE);
    uint64_t sig_raw_target_id = (uint64_t)value_target_id;
    payload = CAN_pack_raw_signal(payload, sig_raw_target_id, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_flags = (uint64_t)value_flags;
    payload = CAN_pack_raw_signal(payload, sig_raw_flags, 0xFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BL_MAIN_MODULE_INFO_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    static const uint32_t sid_list[] = {
        BL_MAIN_MODULE_START_MSG_ID,
        BL_MAIN_MODULE_CRC_MSG_ID,
        BL_MAIN_MODULE_JUMP_MSG_ID,
        BL_MAIN_MODULE_DATA_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN2,
        sid_list, 4,
        NULL, 0
    );
}

#endif // BL_MAIN_MODULE_H
