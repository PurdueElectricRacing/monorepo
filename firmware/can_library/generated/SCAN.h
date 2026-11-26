#ifndef SCAN_H
#define SCAN_H

/**
 * @file SCAN.h
 * @brief Auto-generated CAN bus definitions for SCAN.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#if defined(STM32F423xx)
#include "common/phal_F4/can/can.h"
#elif defined(STM32G474xx)
#include "common/phal_G4/fdcan/fdcan.h"
#else
#error "Unsupported CAN target"
#endif
#include "can_library/generated/can_types.h"

static constexpr uint32_t SCAN_BUS_ID = 3;
static constexpr PHAL_FDCAN_BaudRate_t SCAN_BAUD_RATE = FDCAN_BAUD_500K;

static constexpr uint32_t BL_FRONT_DRIVELINE_START_MSG_ID = 0x2C;
static constexpr uint8_t  BL_FRONT_DRIVELINE_START_DLC = 4;
static constexpr uint64_t BL_FRONT_DRIVELINE_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_front_driveline_start_data_t;

static constexpr uint32_t BL_FRONT_DRIVELINE_DATA_MSG_ID = 0x2D;
static constexpr uint8_t  BL_FRONT_DRIVELINE_DATA_DLC = 7;
static constexpr uint64_t BL_FRONT_DRIVELINE_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_front_driveline_data_data_t;

static constexpr uint32_t BL_FRONT_DRIVELINE_RESP_MSG_ID = 0x2E;
static constexpr uint8_t  BL_FRONT_DRIVELINE_RESP_DLC = 5;
static constexpr uint64_t BL_FRONT_DRIVELINE_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_front_driveline_resp_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_START_MSG_ID = 0x2F;
static constexpr uint8_t  BL_REAR_DRIVELINE_START_DLC = 4;
static constexpr uint64_t BL_REAR_DRIVELINE_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_rear_driveline_start_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_DATA_MSG_ID = 0x30;
static constexpr uint8_t  BL_REAR_DRIVELINE_DATA_DLC = 7;
static constexpr uint64_t BL_REAR_DRIVELINE_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_rear_driveline_data_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_RESP_MSG_ID = 0x31;
static constexpr uint8_t  BL_REAR_DRIVELINE_RESP_DLC = 5;
static constexpr uint64_t BL_REAR_DRIVELINE_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_rear_driveline_resp_data_t;

static constexpr uint32_t BL_FRONT_DRIVELINE_CRC_MSG_ID = 0x36;
static constexpr uint8_t  BL_FRONT_DRIVELINE_CRC_DLC = 4;
static constexpr uint64_t BL_FRONT_DRIVELINE_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_front_driveline_crc_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_CRC_MSG_ID = 0x37;
static constexpr uint8_t  BL_REAR_DRIVELINE_CRC_DLC = 4;
static constexpr uint64_t BL_REAR_DRIVELINE_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_rear_driveline_crc_data_t;

static constexpr uint32_t BL_FRONT_DRIVELINE_JUMP_MSG_ID = 0x3C;
static constexpr uint8_t  BL_FRONT_DRIVELINE_JUMP_DLC = 4;
static constexpr uint64_t BL_FRONT_DRIVELINE_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_front_driveline_jump_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_JUMP_MSG_ID = 0x3D;
static constexpr uint8_t  BL_REAR_DRIVELINE_JUMP_DLC = 4;
static constexpr uint64_t BL_REAR_DRIVELINE_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_rear_driveline_jump_data_t;

static constexpr uint32_t BMS_PECS_MSG_ID = 0x3E;
static constexpr uint8_t  BMS_PECS_DLC = 2;
static constexpr uint64_t BMS_PECS_LAYOUT_HASH = 0xA0950EA1443BECF5;
static constexpr uint32_t BMS_PECS_PERIOD_MS = 200;
static constexpr uint32_t BMS_PECS_STALE_TIMEOUT_MS = 500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool RDCV_0_PEC;
    bool RDCV_1_PEC;
    bool RDCV_2_PEC;
    bool RDCV_3_PEC;
    bool RDCV_4_PEC;
    bool RDCV_5_PEC;
    bool RDAUX_0_PEC;
    bool RDAUX_1_PEC;
    bool RDAUX_2_PEC;
    bool RDAUX_3_PEC;
    // End signal defs
} bms_pecs_data_t;

static constexpr uint32_t FRONT_DRIVELINE_INIT_MSG_ID = 0x3F;
static constexpr uint8_t  FRONT_DRIVELINE_INIT_DLC = 4;
static constexpr uint64_t FRONT_DRIVELINE_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} front_driveline_init_data_t;

static constexpr uint32_t GPS_SPEED_HEADING_MSG_ID = 0x40;
static constexpr uint8_t  GPS_SPEED_HEADING_DLC = 8;
static constexpr uint64_t GPS_SPEED_HEADING_LAYOUT_HASH = 0xB8B6D1B7D2E63360;
static constexpr uint32_t GPS_SPEED_HEADING_PERIOD_MS = 40;
static constexpr uint32_t GPS_SPEED_HEADING_STALE_TIMEOUT_MS = 100;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t gps_speed;
    int32_t gps_heading;
    // End signal defs
} gps_speed_heading_data_t;

static constexpr uint32_t MARK_DATA_MSG_ID = 0x41;
static constexpr uint8_t  MARK_DATA_DLC = 6;
static constexpr uint64_t MARK_DATA_LAYOUT_HASH = 0x0135624341F1A161;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t os_tick;
    uint16_t mark_index;
    // End signal defs
} mark_data_data_t;

static constexpr uint32_t REAR_DRIVELINE_INIT_MSG_ID = 0x42;
static constexpr uint8_t  REAR_DRIVELINE_INIT_DLC = 4;
static constexpr uint64_t REAR_DRIVELINE_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} rear_driveline_init_data_t;

static constexpr uint32_t A_BOX_CAN_OVERFLOW_MSG_ID = 0x43;
static constexpr uint8_t  A_BOX_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t A_BOX_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t A_BOX_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t A_BOX_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} a_box_can_overflow_data_t;

static constexpr uint32_t A_BOX_CAN_TX_ACTIONS_MSG_ID = 0x44;
static constexpr uint8_t  A_BOX_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t A_BOX_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t A_BOX_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t A_BOX_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} a_box_can_tx_actions_data_t;

static constexpr uint32_t A_BOX_CAN_TX_MESSAGES_MSG_ID = 0x45;
static constexpr uint8_t  A_BOX_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t A_BOX_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t A_BOX_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t A_BOX_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} a_box_can_tx_messages_data_t;

static constexpr uint32_t COOLING_OUTPUTS_MSG_ID = 0x46;
static constexpr uint8_t  COOLING_OUTPUTS_DLC = 1;
static constexpr uint64_t COOLING_OUTPUTS_LAYOUT_HASH = 0x919F05979CC87C56;
static constexpr uint32_t COOLING_OUTPUTS_PERIOD_MS = 1000;
static constexpr uint32_t COOLING_OUTPUTS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool is_powertrain_pumps_enabled;
    bool is_hx_fan_enabled;
    bool is_batt_fans_enabled;
    // End signal defs
} cooling_outputs_data_t;

static constexpr uint32_t DASH_CAN_OVERFLOW_MSG_ID = 0x47;
static constexpr uint8_t  DASH_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t DASH_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t DASH_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t DASH_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} dash_can_overflow_data_t;

static constexpr uint32_t DASH_CAN_TX_ACTIONS_MSG_ID = 0x48;
static constexpr uint8_t  DASH_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t DASH_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t DASH_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t DASH_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} dash_can_tx_actions_data_t;

static constexpr uint32_t DASH_CAN_TX_MESSAGES_MSG_ID = 0x49;
static constexpr uint8_t  DASH_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t DASH_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t DASH_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t DASH_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} dash_can_tx_messages_data_t;

static constexpr uint32_t FRONT_DRIVELINE_CAN_OVERFLOW_MSG_ID = 0x4A;
static constexpr uint8_t  FRONT_DRIVELINE_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t FRONT_DRIVELINE_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t FRONT_DRIVELINE_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t FRONT_DRIVELINE_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} front_driveline_can_overflow_data_t;

static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_ACTIONS_MSG_ID = 0x4B;
static constexpr uint8_t  FRONT_DRIVELINE_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t FRONT_DRIVELINE_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} front_driveline_can_tx_actions_data_t;

static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_MESSAGES_MSG_ID = 0x4C;
static constexpr uint8_t  FRONT_DRIVELINE_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t FRONT_DRIVELINE_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t FRONT_DRIVELINE_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} front_driveline_can_tx_messages_data_t;

static constexpr uint32_t FRONT_DRIVELINE_VERSION_MSG_ID = 0x4D;
static constexpr uint8_t  FRONT_DRIVELINE_VERSION_DLC = 8;
static constexpr uint64_t FRONT_DRIVELINE_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t FRONT_DRIVELINE_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t FRONT_DRIVELINE_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} front_driveline_version_data_t;

static constexpr uint32_t FRONT_OIL_TEMPS_MSG_ID = 0x4E;
static constexpr uint8_t  FRONT_OIL_TEMPS_DLC = 4;
static constexpr uint64_t FRONT_OIL_TEMPS_LAYOUT_HASH = 0xF79A7E8FBCF8577A;
static constexpr uint32_t FRONT_OIL_TEMPS_PERIOD_MS = 4000;
static constexpr uint32_t FRONT_OIL_TEMPS_STALE_TIMEOUT_MS = 10000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t left;
    int16_t right;
    // End signal defs
} front_oil_temps_data_t;

static constexpr uint32_t IMD_STATE_MSG_ID = 0x4F;
static constexpr uint8_t  IMD_STATE_DLC = 5;
static constexpr uint64_t IMD_STATE_LAYOUT_HASH = 0xEAAB60A5A7445132;
static constexpr uint32_t IMD_STATE_PERIOD_MS = 1000;
static constexpr uint32_t IMD_STATE_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    imd_state_t imd_state;
    bool imd_status;
    imd_r_iso_quality_t imd_r_iso_quality;
    uint16_t r_iso;
    // End signal defs
} imd_state_data_t;

static constexpr uint32_t PDU_CAN_OVERFLOW_MSG_ID = 0x50;
static constexpr uint8_t  PDU_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t PDU_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t PDU_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t PDU_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} pdu_can_overflow_data_t;

static constexpr uint32_t PDU_CAN_TX_ACTIONS_MSG_ID = 0x51;
static constexpr uint8_t  PDU_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t PDU_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t PDU_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t PDU_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} pdu_can_tx_actions_data_t;

static constexpr uint32_t PDU_CAN_TX_MESSAGES_MSG_ID = 0x52;
static constexpr uint8_t  PDU_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t PDU_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t PDU_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t PDU_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} pdu_can_tx_messages_data_t;

static constexpr uint32_t REAR_DRIVELINE_CAN_OVERFLOW_MSG_ID = 0x53;
static constexpr uint8_t  REAR_DRIVELINE_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t REAR_DRIVELINE_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t REAR_DRIVELINE_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t REAR_DRIVELINE_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} rear_driveline_can_overflow_data_t;

static constexpr uint32_t REAR_DRIVELINE_CAN_TX_ACTIONS_MSG_ID = 0x54;
static constexpr uint8_t  REAR_DRIVELINE_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t REAR_DRIVELINE_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t REAR_DRIVELINE_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t REAR_DRIVELINE_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} rear_driveline_can_tx_actions_data_t;

static constexpr uint32_t REAR_DRIVELINE_CAN_TX_MESSAGES_MSG_ID = 0x55;
static constexpr uint8_t  REAR_DRIVELINE_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t REAR_DRIVELINE_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t REAR_DRIVELINE_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t REAR_DRIVELINE_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} rear_driveline_can_tx_messages_data_t;

static constexpr uint32_t REAR_DRIVELINE_VERSION_MSG_ID = 0x56;
static constexpr uint8_t  REAR_DRIVELINE_VERSION_DLC = 8;
static constexpr uint64_t REAR_DRIVELINE_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t REAR_DRIVELINE_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t REAR_DRIVELINE_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} rear_driveline_version_data_t;

static constexpr uint32_t REAR_OIL_TEMPS_MSG_ID = 0x57;
static constexpr uint8_t  REAR_OIL_TEMPS_DLC = 4;
static constexpr uint64_t REAR_OIL_TEMPS_LAYOUT_HASH = 0xF79A7E8FBCF8577A;
static constexpr uint32_t REAR_OIL_TEMPS_PERIOD_MS = 4000;
static constexpr uint32_t REAR_OIL_TEMPS_STALE_TIMEOUT_MS = 10000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t left;
    int16_t right;
    // End signal defs
} rear_oil_temps_data_t;

static constexpr uint32_t TORQUE_VECTOR_CAN_OVERFLOW_MSG_ID = 0x58;
static constexpr uint8_t  TORQUE_VECTOR_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t TORQUE_VECTOR_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t TORQUE_VECTOR_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t TORQUE_VECTOR_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} torque_vector_can_overflow_data_t;

static constexpr uint32_t TORQUE_VECTOR_CAN_TX_ACTIONS_MSG_ID = 0x59;
static constexpr uint8_t  TORQUE_VECTOR_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t TORQUE_VECTOR_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t TORQUE_VECTOR_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t TORQUE_VECTOR_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} torque_vector_can_tx_actions_data_t;

static constexpr uint32_t TORQUE_VECTOR_CAN_TX_MESSAGES_MSG_ID = 0x5A;
static constexpr uint8_t  TORQUE_VECTOR_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t TORQUE_VECTOR_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t TORQUE_VECTOR_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t TORQUE_VECTOR_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} torque_vector_can_tx_messages_data_t;

static constexpr uint32_t BL_FRONT_DRIVELINE_INFO_MSG_ID = 0x104;
static constexpr uint8_t  BL_FRONT_DRIVELINE_INFO_DLC = 7;
static constexpr uint64_t BL_FRONT_DRIVELINE_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_FRONT_DRIVELINE_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_FRONT_DRIVELINE_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_front_driveline_info_data_t;

static constexpr uint32_t BL_REAR_DRIVELINE_INFO_MSG_ID = 0x105;
static constexpr uint8_t  BL_REAR_DRIVELINE_INFO_DLC = 7;
static constexpr uint64_t BL_REAR_DRIVELINE_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_REAR_DRIVELINE_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_REAR_DRIVELINE_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_rear_driveline_info_data_t;

static constexpr uint32_t IMU_ACCELERATION_MSG_ID = 0x106;
static constexpr uint8_t  IMU_ACCELERATION_DLC = 8;
static constexpr uint64_t IMU_ACCELERATION_LAYOUT_HASH = 0x8CC7E4AA8DA5EE11;
static constexpr uint32_t IMU_ACCELERATION_PERIOD_MS = 10;
static constexpr uint32_t IMU_ACCELERATION_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t X_axis;
    int16_t Y_axis;
    int16_t Z_axis;
    int16_t temperature;
    // End signal defs
} IMU_acceleration_data_t;

static constexpr uint32_t IMU_ANGULAR_RATE_MSG_ID = 0x107;
static constexpr uint8_t  IMU_ANGULAR_RATE_DLC = 8;
static constexpr uint64_t IMU_ANGULAR_RATE_LAYOUT_HASH = 0x1B1AAEFC1A933FFF;
static constexpr uint32_t IMU_ANGULAR_RATE_PERIOD_MS = 10;
static constexpr uint32_t IMU_ANGULAR_RATE_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t X_axis;
    int16_t Y_axis;
    int16_t Z_axis;
    uint16_t reserved;
    // End signal defs
} IMU_angular_rate_data_t;

static constexpr uint32_t BRAKE_PRESSURE_MSG_ID = 0x108;
static constexpr uint8_t  BRAKE_PRESSURE_DLC = 2;
static constexpr uint64_t BRAKE_PRESSURE_LAYOUT_HASH = 0x0430064720D52DA0;
static constexpr uint32_t BRAKE_PRESSURE_PERIOD_MS = 40;
static constexpr uint32_t BRAKE_PRESSURE_STALE_TIMEOUT_MS = 100;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t brake_pressure;
    // End signal defs
} brake_pressure_data_t;

static constexpr uint32_t CELL_TELEMETRY_MSG_ID = 0x109;
static constexpr uint8_t  CELL_TELEMETRY_DLC = 5;
static constexpr uint64_t CELL_TELEMETRY_LAYOUT_HASH = 0xE1D5FC2D60B4D808;
static constexpr uint32_t CELL_TELEMETRY_PERIOD_MS = 10;
static constexpr uint32_t CELL_TELEMETRY_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t voltage;
    uint8_t module_num;
    uint8_t cell_num;
    bool balance_status;
    // End signal defs
} cell_telemetry_data_t;

static constexpr uint32_t FRONT_SHOCKPOTS_MSG_ID = 0x10A;
static constexpr uint8_t  FRONT_SHOCKPOTS_DLC = 4;
static constexpr uint64_t FRONT_SHOCKPOTS_LAYOUT_HASH = 0x2554A97E03B4BCA8;
static constexpr uint32_t FRONT_SHOCKPOTS_PERIOD_MS = 10;
static constexpr uint32_t FRONT_SHOCKPOTS_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t left;
    uint16_t right;
    // End signal defs
} front_shockpots_data_t;

static constexpr uint32_t GPS_COORDINATES_MSG_ID = 0x10B;
static constexpr uint8_t  GPS_COORDINATES_DLC = 8;
static constexpr uint64_t GPS_COORDINATES_LAYOUT_HASH = 0x23CF4FA252D62523;
static constexpr uint32_t GPS_COORDINATES_PERIOD_MS = 40;
static constexpr uint32_t GPS_COORDINATES_STALE_TIMEOUT_MS = 100;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t latitude;
    int32_t longitude;
    // End signal defs
} gps_coordinates_data_t;

static constexpr uint32_t GPS_VELOCITY_MSG_ID = 0x10C;
static constexpr uint8_t  GPS_VELOCITY_DLC = 8;
static constexpr uint64_t GPS_VELOCITY_LAYOUT_HASH = 0x320443BDA70F88D8;
static constexpr uint32_t GPS_VELOCITY_PERIOD_MS = 40;
static constexpr uint32_t GPS_VELOCITY_STALE_TIMEOUT_MS = 100;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t gps_vel_n;
    int32_t gps_vel_e;
    // End signal defs
} gps_velocity_data_t;

static constexpr uint32_t E_METER_MEASUREMENT_MSG_ID = 0x10D;
static constexpr uint8_t  E_METER_MEASUREMENT_DLC = 8;
static constexpr uint64_t E_METER_MEASUREMENT_LAYOUT_HASH = 0x564753C53F08D4F4;
static constexpr uint32_t E_METER_MEASUREMENT_PERIOD_MS = 20;
static constexpr uint32_t E_METER_MEASUREMENT_STALE_TIMEOUT_MS = 50;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    float current;
    float voltage;
    // End signal defs
} e_meter_measurement_data_t;

static constexpr uint32_t REAR_SHOCKPOTS_MSG_ID = 0x10E;
static constexpr uint8_t  REAR_SHOCKPOTS_DLC = 4;
static constexpr uint64_t REAR_SHOCKPOTS_LAYOUT_HASH = 0x2554A97E03B4BCA8;
static constexpr uint32_t REAR_SHOCKPOTS_PERIOD_MS = 10;
static constexpr uint32_t REAR_SHOCKPOTS_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t left;
    uint16_t right;
    // End signal defs
} rear_shockpots_data_t;

static constexpr uint32_t SWITCH_CS_MSG_ID = 0x10F;
static constexpr uint8_t  SWITCH_CS_DLC = 3;
static constexpr uint64_t SWITCH_CS_LAYOUT_HASH = 0xA2B0E1314DFAEEB7;
static constexpr uint32_t SWITCH_CS_PERIOD_MS = 4;
static constexpr uint32_t SWITCH_CS_STALE_TIMEOUT_MS = 10;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    switch_id_t switch_id;
    uint16_t current;
    // End signal defs
} switch_cs_data_t;

static constexpr uint32_t THERMISTOR_TELEMETRY_MSG_ID = 0x110;
static constexpr uint8_t  THERMISTOR_TELEMETRY_DLC = 5;
static constexpr uint64_t THERMISTOR_TELEMETRY_LAYOUT_HASH = 0x0E727A25C2834112;
static constexpr uint32_t THERMISTOR_TELEMETRY_PERIOD_MS = 125;
static constexpr uint32_t THERMISTOR_TELEMETRY_STALE_TIMEOUT_MS = 312;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t temperature;
    uint8_t module_num;
    uint8_t thermistor_num;
    bool is_board_temp;
    // End signal defs
} thermistor_telemetry_data_t;

static constexpr uint32_t E_METER_STATUS_MSG_ID = 0x40D;
static constexpr uint8_t  E_METER_STATUS_DLC = 8;
static constexpr uint64_t E_METER_STATUS_LAYOUT_HASH = 0x34AAFED6D021C8DA;
static constexpr uint32_t E_METER_STATUS_PERIOD_MS = 100;
static constexpr uint32_t E_METER_STATUS_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool violation;
    bool logging;
    bool fault_active;
    bool fault_prev_active;
    bool reserved_bool0;
    bool reserved_bool1;
    bool reserved_bool2;
    bool reserved_bool3;
    float energy;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    // End signal defs
} e_meter_status_data_t;

static constexpr uint32_t IVT_S_CURRENT_MSG_ID = 0x521;
static constexpr uint8_t  IVT_S_CURRENT_DLC = 6;
static constexpr uint64_t IVT_S_CURRENT_LAYOUT_HASH = 0x4A30837C08BA9B46;
static constexpr uint32_t IVT_S_CURRENT_PERIOD_MS = 20;
static constexpr uint32_t IVT_S_CURRENT_STALE_TIMEOUT_MS = 50;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t current;
    // End signal defs
} ivt_s_current_data_t;

static constexpr uint32_t IVT_S_U1_MSG_ID = 0x522;
static constexpr uint8_t  IVT_S_U1_DLC = 6;
static constexpr uint64_t IVT_S_U1_LAYOUT_HASH = 0xA7988EA9CCD49553;
static constexpr uint32_t IVT_S_U1_PERIOD_MS = 60;
static constexpr uint32_t IVT_S_U1_STALE_TIMEOUT_MS = 150;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t voltage;
    // End signal defs
} ivt_s_u1_data_t;

static constexpr uint32_t IVT_S_TEMPERATURE_MSG_ID = 0x525;
static constexpr uint8_t  IVT_S_TEMPERATURE_DLC = 6;
static constexpr uint64_t IVT_S_TEMPERATURE_LAYOUT_HASH = 0x62BBEF8662856E81;
static constexpr uint32_t IVT_S_TEMPERATURE_PERIOD_MS = 100;
static constexpr uint32_t IVT_S_TEMPERATURE_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t temperature;
    // End signal defs
} ivt_s_temperature_data_t;

static constexpr uint32_t IVT_S_POWER_MSG_ID = 0x526;
static constexpr uint8_t  IVT_S_POWER_DLC = 6;
static constexpr uint64_t IVT_S_POWER_LAYOUT_HASH = 0xA627CB6642BAA5D9;
static constexpr uint32_t IVT_S_POWER_PERIOD_MS = 30;
static constexpr uint32_t IVT_S_POWER_STALE_TIMEOUT_MS = 75;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t power;
    // End signal defs
} ivt_s_power_data_t;

static constexpr uint32_t IVT_S_CHARGE_MSG_ID = 0x527;
static constexpr uint8_t  IVT_S_CHARGE_DLC = 6;
static constexpr uint64_t IVT_S_CHARGE_LAYOUT_HASH = 0x56E9300F137E2099;
static constexpr uint32_t IVT_S_CHARGE_PERIOD_MS = 30;
static constexpr uint32_t IVT_S_CHARGE_STALE_TIMEOUT_MS = 75;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t charge;
    // End signal defs
} ivt_s_charge_data_t;

static constexpr uint32_t IVT_S_ENERGY_MSG_ID = 0x528;
static constexpr uint8_t  IVT_S_ENERGY_DLC = 6;
static constexpr uint64_t IVT_S_ENERGY_LAYOUT_HASH = 0x949ED3B726F02EAC;
static constexpr uint32_t IVT_S_ENERGY_PERIOD_MS = 30;
static constexpr uint32_t IVT_S_ENERGY_STALE_TIMEOUT_MS = 75;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t mux_id;
    uint8_t message_count;
    bool overcurrent;
    bool result_warning;
    bool measurement_error;
    bool system_error;
    int32_t energy;
    // End signal defs
} ivt_s_energy_data_t;

static constexpr uint32_t E_METER_TEMPERATURE_MSG_ID = 0x60D;
static constexpr uint8_t  E_METER_TEMPERATURE_DLC = 8;
static constexpr uint64_t E_METER_TEMPERATURE_LAYOUT_HASH = 0xE415F499418FC824;
static constexpr uint32_t E_METER_TEMPERATURE_PERIOD_MS = 125;
static constexpr uint32_t E_METER_TEMPERATURE_STALE_TIMEOUT_MS = 312;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t max_cell_temp;
    uint16_t min_cell_temp;
    uint16_t avg_cell_temp;
    uint8_t reserved1;
    uint8_t reserved2;
    // End signal defs
} e_meter_temperature_data_t;


#endif // SCAN_H
