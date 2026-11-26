#ifndef VCAN_H
#define VCAN_H

/**
 * @file VCAN.h
 * @brief Auto-generated CAN bus definitions for VCAN.
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

static constexpr uint32_t VCAN_BUS_ID = 0;
static constexpr PHAL_FDCAN_BaudRate_t VCAN_BAUD_RATE = FDCAN_BAUD_500K;

static constexpr uint32_t A_BOX_FAULT_EVENT_MSG_ID = 0x1;
static constexpr uint8_t  A_BOX_FAULT_EVENT_DLC = 5;
static constexpr uint64_t A_BOX_FAULT_EVENT_LAYOUT_HASH = 0xE273EB95759AA897;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    fault_id_t idx;
    uint16_t val;
    bool state;
    // End signal defs
} a_box_fault_event_data_t;

static constexpr uint32_t ABOX_INIT_MSG_ID = 0x2;
static constexpr uint8_t  ABOX_INIT_DLC = 4;
static constexpr uint64_t ABOX_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} abox_init_data_t;

static constexpr uint32_t DASH_INIT_MSG_ID = 0x3;
static constexpr uint8_t  DASH_INIT_DLC = 4;
static constexpr uint64_t DASH_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} dash_init_data_t;

static constexpr uint32_t DASHBOARD_FAULT_EVENT_MSG_ID = 0x4;
static constexpr uint8_t  DASHBOARD_FAULT_EVENT_DLC = 5;
static constexpr uint64_t DASHBOARD_FAULT_EVENT_LAYOUT_HASH = 0xE273EB95759AA897;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    fault_id_t idx;
    uint16_t val;
    bool state;
    // End signal defs
} dashboard_fault_event_data_t;

static constexpr uint32_t MAIN_INIT_MSG_ID = 0x5;
static constexpr uint8_t  MAIN_INIT_DLC = 4;
static constexpr uint64_t MAIN_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} main_init_data_t;

static constexpr uint32_t MAIN_MODULE_FAULT_EVENT_MSG_ID = 0x6;
static constexpr uint8_t  MAIN_MODULE_FAULT_EVENT_DLC = 5;
static constexpr uint64_t MAIN_MODULE_FAULT_EVENT_LAYOUT_HASH = 0xE273EB95759AA897;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    fault_id_t idx;
    uint16_t val;
    bool state;
    // End signal defs
} main_module_fault_event_data_t;

static constexpr uint32_t PDU_FAULT_EVENT_MSG_ID = 0x7;
static constexpr uint8_t  PDU_FAULT_EVENT_DLC = 5;
static constexpr uint64_t PDU_FAULT_EVENT_LAYOUT_HASH = 0xE273EB95759AA897;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    fault_id_t idx;
    uint16_t val;
    bool state;
    // End signal defs
} pdu_fault_event_data_t;

static constexpr uint32_t PDU_INIT_MSG_ID = 0x8;
static constexpr uint8_t  PDU_INIT_DLC = 4;
static constexpr uint64_t PDU_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} pdu_init_data_t;

static constexpr uint32_t TORQUE_VECTOR_FAULT_EVENT_MSG_ID = 0x9;
static constexpr uint8_t  TORQUE_VECTOR_FAULT_EVENT_DLC = 5;
static constexpr uint64_t TORQUE_VECTOR_FAULT_EVENT_LAYOUT_HASH = 0xE273EB95759AA897;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    fault_id_t idx;
    uint16_t val;
    bool state;
    // End signal defs
} torque_vector_fault_event_data_t;

static constexpr uint32_t TV_INIT_MSG_ID = 0xA;
static constexpr uint8_t  TV_INIT_DLC = 4;
static constexpr uint64_t TV_INIT_LAYOUT_HASH = 0x3D0CEB151300DA0E;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t csr;
    // End signal defs
} tv_init_data_t;

static constexpr uint32_t BL_MAIN_MODULE_START_MSG_ID = 0x20;
static constexpr uint8_t  BL_MAIN_MODULE_START_DLC = 4;
static constexpr uint64_t BL_MAIN_MODULE_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_main_module_start_data_t;

static constexpr uint32_t BL_MAIN_MODULE_DATA_MSG_ID = 0x21;
static constexpr uint8_t  BL_MAIN_MODULE_DATA_DLC = 7;
static constexpr uint64_t BL_MAIN_MODULE_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_main_module_data_data_t;

static constexpr uint32_t BL_MAIN_MODULE_RESP_MSG_ID = 0x22;
static constexpr uint8_t  BL_MAIN_MODULE_RESP_DLC = 5;
static constexpr uint64_t BL_MAIN_MODULE_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_main_module_resp_data_t;

static constexpr uint32_t BL_DASHBOARD_START_MSG_ID = 0x23;
static constexpr uint8_t  BL_DASHBOARD_START_DLC = 4;
static constexpr uint64_t BL_DASHBOARD_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_dashboard_start_data_t;

static constexpr uint32_t BL_DASHBOARD_DATA_MSG_ID = 0x24;
static constexpr uint8_t  BL_DASHBOARD_DATA_DLC = 7;
static constexpr uint64_t BL_DASHBOARD_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_dashboard_data_data_t;

static constexpr uint32_t BL_DASHBOARD_RESP_MSG_ID = 0x25;
static constexpr uint8_t  BL_DASHBOARD_RESP_DLC = 5;
static constexpr uint64_t BL_DASHBOARD_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_dashboard_resp_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_START_MSG_ID = 0x26;
static constexpr uint8_t  BL_TORQUE_VECTOR_START_DLC = 4;
static constexpr uint64_t BL_TORQUE_VECTOR_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_torque_vector_start_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_DATA_MSG_ID = 0x27;
static constexpr uint8_t  BL_TORQUE_VECTOR_DATA_DLC = 7;
static constexpr uint64_t BL_TORQUE_VECTOR_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_torque_vector_data_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_RESP_MSG_ID = 0x28;
static constexpr uint8_t  BL_TORQUE_VECTOR_RESP_DLC = 5;
static constexpr uint64_t BL_TORQUE_VECTOR_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_torque_vector_resp_data_t;

static constexpr uint32_t BL_A_BOX_START_MSG_ID = 0x29;
static constexpr uint8_t  BL_A_BOX_START_DLC = 4;
static constexpr uint64_t BL_A_BOX_START_LAYOUT_HASH = 0xDEAF28FDA475CB73;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t image_size_bytes;
    // End signal defs
} bl_a_box_start_data_t;

static constexpr uint32_t BL_A_BOX_DATA_MSG_ID = 0x2A;
static constexpr uint8_t  BL_A_BOX_DATA_DLC = 7;
static constexpr uint64_t BL_A_BOX_DATA_LAYOUT_HASH = 0x4CB21F018283CF80;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t index;
    uint32_t data;
    // End signal defs
} bl_a_box_data_data_t;

static constexpr uint32_t BL_A_BOX_RESP_MSG_ID = 0x2B;
static constexpr uint8_t  BL_A_BOX_RESP_DLC = 5;
static constexpr uint64_t BL_A_BOX_RESP_LAYOUT_HASH = 0xAA84CFE4544C64AC;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    bootloader_status_t status;
    uint32_t detail;
    // End signal defs
} bl_a_box_resp_data_t;

static constexpr uint32_t BL_MAIN_MODULE_CRC_MSG_ID = 0x32;
static constexpr uint8_t  BL_MAIN_MODULE_CRC_DLC = 4;
static constexpr uint64_t BL_MAIN_MODULE_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_main_module_crc_data_t;

static constexpr uint32_t BL_DASHBOARD_CRC_MSG_ID = 0x33;
static constexpr uint8_t  BL_DASHBOARD_CRC_DLC = 4;
static constexpr uint64_t BL_DASHBOARD_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_dashboard_crc_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_CRC_MSG_ID = 0x34;
static constexpr uint8_t  BL_TORQUE_VECTOR_CRC_DLC = 4;
static constexpr uint64_t BL_TORQUE_VECTOR_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_torque_vector_crc_data_t;

static constexpr uint32_t BL_A_BOX_CRC_MSG_ID = 0x35;
static constexpr uint8_t  BL_A_BOX_CRC_DLC = 4;
static constexpr uint64_t BL_A_BOX_CRC_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_a_box_crc_data_t;

static constexpr uint32_t BL_MAIN_MODULE_JUMP_MSG_ID = 0x38;
static constexpr uint8_t  BL_MAIN_MODULE_JUMP_DLC = 4;
static constexpr uint64_t BL_MAIN_MODULE_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_main_module_jump_data_t;

static constexpr uint32_t BL_DASHBOARD_JUMP_MSG_ID = 0x39;
static constexpr uint8_t  BL_DASHBOARD_JUMP_DLC = 4;
static constexpr uint64_t BL_DASHBOARD_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_dashboard_jump_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_JUMP_MSG_ID = 0x3A;
static constexpr uint8_t  BL_TORQUE_VECTOR_JUMP_DLC = 4;
static constexpr uint64_t BL_TORQUE_VECTOR_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_torque_vector_jump_data_t;

static constexpr uint32_t BL_A_BOX_JUMP_MSG_ID = 0x3B;
static constexpr uint8_t  BL_A_BOX_JUMP_DLC = 4;
static constexpr uint64_t BL_A_BOX_JUMP_LAYOUT_HASH = 0xDF654BBB5645F984;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint32_t data;
    // End signal defs
} bl_a_box_jump_data_t;

static constexpr uint32_t A_BOX_FAULT_SYNC_MSG_ID = 0x3C;
static constexpr uint8_t  A_BOX_FAULT_SYNC_DLC = 2;
static constexpr uint64_t A_BOX_FAULT_SYNC_LAYOUT_HASH = 0x09D8DA0AC4A3F623;
static constexpr uint32_t A_BOX_FAULT_SYNC_PERIOD_MS = 100;
static constexpr uint32_t A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool PACK_OVERTEMP;
    bool PACK_WARM;
    bool PACK_COLD;
    bool IMD;
    bool BMS_DISCONNECTED;
    bool CELL_UNDERVOLTAGE;
    bool CELL_OVERVOLTAGE;
    bool PACK_FULL;
    bool PACK_EMPTY;
    // End signal defs
} a_box_fault_sync_data_t;

static constexpr uint32_t BMS_TEMPS_MSG_ID = 0x3D;
static constexpr uint8_t  BMS_TEMPS_DLC = 6;
static constexpr uint64_t BMS_TEMPS_LAYOUT_HASH = 0x4776F0D21CB8D88F;
static constexpr uint32_t BMS_TEMPS_PERIOD_MS = 200;
static constexpr uint32_t BMS_TEMPS_STALE_TIMEOUT_MS = 500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t min_cell_temp;
    uint8_t max_cell_temp;
    uint8_t avg_cell_temp;
    uint8_t min_board_temp;
    uint8_t max_board_temp;
    uint8_t avg_board_temp;
    // End signal defs
} bms_temps_data_t;

static constexpr uint32_t BMS_VOLTAGES_MSG_ID = 0x3E;
static constexpr uint8_t  BMS_VOLTAGES_DLC = 8;
static constexpr uint64_t BMS_VOLTAGES_LAYOUT_HASH = 0x3B8969C6E5AD8494;
static constexpr uint32_t BMS_VOLTAGES_PERIOD_MS = 200;
static constexpr uint32_t BMS_VOLTAGES_STALE_TIMEOUT_MS = 500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t pack_voltage;
    uint16_t min_cell_voltage;
    uint16_t max_cell_voltage;
    uint16_t avg_cell_voltage;
    // End signal defs
} bms_voltages_data_t;

static constexpr uint32_t DASHBOARD_FAULT_SYNC_MSG_ID = 0x3F;
static constexpr uint8_t  DASHBOARD_FAULT_SYNC_DLC = 1;
static constexpr uint64_t DASHBOARD_FAULT_SYNC_LAYOUT_HASH = 0xC826F64110FD037F;
static constexpr uint32_t DASHBOARD_FAULT_SYNC_PERIOD_MS = 100;
static constexpr uint32_t DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool APPS_wiring_t1;
    bool APPS_wiring_t2;
    bool APPS_IMPLAUSIBLE;
    bool APPS_BRAKE;
    bool BSE;
    // End signal defs
} dashboard_fault_sync_data_t;

static constexpr uint32_t MAIN_MODULE_FAULT_SYNC_MSG_ID = 0x40;
static constexpr uint8_t  MAIN_MODULE_FAULT_SYNC_DLC = 3;
static constexpr uint64_t MAIN_MODULE_FAULT_SYNC_LAYOUT_HASH = 0x0C23F81D934442E3;
static constexpr uint32_t MAIN_MODULE_FAULT_SYNC_PERIOD_MS = 100;
static constexpr uint32_t MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool SDC1_IMD;
    bool SDC2_BMS;
    bool SDC3_BSPD;
    bool SDC4_ECU_OK;
    bool SDC5_BOTS;
    bool SDC6_INERTIA;
    bool SDC7_COCKPIT_ESTOP;
    bool SDC9_FRONT_INTERLOCK;
    bool SDC10_RIGHT_ESTOP;
    bool SDC11_LEFT_ESTOP;
    bool SDC12_MSD;
    bool SDC13_E_METER;
    bool SDC14_HVD;
    bool SDC16_REAR_INTERLOCK;
    bool SDC17_TSMS;
    bool SDC19_PRECHARGE_COMPLETE;
    bool PRECHARGE_INCOMPLETE;
    // End signal defs
} main_module_fault_sync_data_t;

static constexpr uint32_t PDU_FAULT_SYNC_MSG_ID = 0x41;
static constexpr uint8_t  PDU_FAULT_SYNC_DLC = 3;
static constexpr uint64_t PDU_FAULT_SYNC_LAYOUT_HASH = 0x3263D0C508DE8B65;
static constexpr uint32_t PDU_FAULT_SYNC_PERIOD_MS = 100;
static constexpr uint32_t PDU_FAULT_SYNC_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool SDC_SWITCH_FAULT;
    bool SDC_LOOP_SWITCH_FAULT;
    bool MAIN_SWITCH_FAULT;
    bool DASH_SWITCH_FAULT;
    bool DAQ_SWITCH_FAULT;
    bool ABOX_SWITCH_FAULT;
    bool HV_BOARD_SWITCH_FAULT;
    bool TV_SWITCH_FAULT;
    bool DL_FRONT_SWITCH_FAULT;
    bool DL_REAR_SWITCH_FAULT;
    bool AMK1_SWITCH_FAULT;
    bool AMK2_SWITCH_FAULT;
    bool PUMP1_SWITCH_FAULT;
    bool PUMP2_SWITCH_FAULT;
    bool HXFAN_SWITCH_FAULT;
    bool FAN1_SWITCH_FAULT;
    bool FAN2_SWITCH_FAULT;
    bool FAN3_SWITCH_FAULT;
    bool FAN4_SWITCH_FAULT;
    bool FAN5_SWITCH_FAULT;
    bool EMETER_SWITCH_FAULT;
    // End signal defs
} pdu_fault_sync_data_t;

static constexpr uint32_t START_BUTTON_MSG_ID = 0x42;
static constexpr uint8_t  START_BUTTON_DLC = 1;
static constexpr uint64_t START_BUTTON_LAYOUT_HASH = 0x76B454147449B2EC;
static constexpr uint32_t START_BUTTON_PERIOD_MS = 200;
static constexpr uint32_t START_BUTTON_STALE_TIMEOUT_MS = 500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool is_pressed;
    // End signal defs
} start_button_data_t;

static constexpr uint32_t TORQUE_VECTOR_FAULT_SYNC_MSG_ID = 0x43;
static constexpr uint8_t  TORQUE_VECTOR_FAULT_SYNC_DLC = 1;
static constexpr uint64_t TORQUE_VECTOR_FAULT_SYNC_LAYOUT_HASH = 0x6988B973FD2317A0;
static constexpr uint32_t TORQUE_VECTOR_FAULT_SYNC_PERIOD_MS = 100;
static constexpr uint32_t TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    bool GPS_INVALID_FIX;
    bool GPS_WEAK_FIX;
    bool GPS_INVALID_UTC;
    // End signal defs
} torque_vector_fault_sync_data_t;

static constexpr uint32_t MAIN_HB_MSG_ID = 0x44;
static constexpr uint8_t  MAIN_HB_DLC = 1;
static constexpr uint64_t MAIN_HB_LAYOUT_HASH = 0xC8524E1A9C6A5A29;
static constexpr uint32_t MAIN_HB_PERIOD_MS = 500;
static constexpr uint32_t MAIN_HB_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    car_state_t car_state;
    // End signal defs
} main_hb_data_t;

static constexpr uint32_t PEDALS_MSG_ID = 0x45;
static constexpr uint8_t  PEDALS_DLC = 3;
static constexpr uint64_t PEDALS_LAYOUT_HASH = 0x8717EE40515B884F;
static constexpr uint32_t PEDALS_PERIOD_MS = 15;
static constexpr uint32_t PEDALS_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t throttle;
    uint8_t regen;
    uint8_t brake;
    // End signal defs
} pedals_data_t;

static constexpr uint32_t VCU_TORQUE_REQUEST_MSG_ID = 0x46;
static constexpr uint8_t  VCU_TORQUE_REQUEST_DLC = 8;
static constexpr uint64_t VCU_TORQUE_REQUEST_LAYOUT_HASH = 0x6E37809D0C79B94A;
static constexpr uint32_t VCU_TORQUE_REQUEST_PERIOD_MS = 15;
static constexpr uint32_t VCU_TORQUE_REQUEST_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t front_right;
    int16_t front_left;
    int16_t rear_left;
    int16_t rear_right;
    // End signal defs
} vcu_torque_request_data_t;

static constexpr uint32_t VCU_DRIVER_REQUEST_MSG_ID = 0x47;
static constexpr uint8_t  VCU_DRIVER_REQUEST_DLC = 5;
static constexpr uint64_t VCU_DRIVER_REQUEST_LAYOUT_HASH = 0xD33922BE6FE9B1A9;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    vcu_mode_t vcu_mode;
    uint8_t lateral_gain;
    uint8_t longitudinal_gain;
    uint8_t electronic_brake_bias;
    bool is_regen_enabled;
    bool is_tv_enabled;
    // End signal defs
} vcu_driver_request_data_t;

static constexpr uint32_t VCU_SETTINGS_MSG_ID = 0x48;
static constexpr uint8_t  VCU_SETTINGS_DLC = 5;
static constexpr uint64_t VCU_SETTINGS_LAYOUT_HASH = 0xD33922BE6FE9B1A9;
static constexpr uint32_t VCU_SETTINGS_PERIOD_MS = 1000;
static constexpr uint32_t VCU_SETTINGS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    vcu_mode_t vcu_mode;
    uint8_t lateral_gain;
    uint8_t longitudinal_gain;
    uint8_t electronic_brake_bias;
    bool is_regen_enabled;
    bool is_tv_enabled;
    // End signal defs
} vcu_settings_data_t;

static constexpr uint32_t ABOX_VERSION_MSG_ID = 0x49;
static constexpr uint8_t  ABOX_VERSION_DLC = 8;
static constexpr uint64_t ABOX_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t ABOX_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t ABOX_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} abox_version_data_t;

static constexpr uint32_t DASH_VERSION_MSG_ID = 0x4A;
static constexpr uint8_t  DASH_VERSION_DLC = 8;
static constexpr uint64_t DASH_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t DASH_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t DASH_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} dash_version_data_t;

static constexpr uint32_t GPS_TIME_MSG_ID = 0x4B;
static constexpr uint8_t  GPS_TIME_DLC = 8;
static constexpr uint64_t GPS_TIME_LAYOUT_HASH = 0xF1DFD6C07E8A2056;
static constexpr uint32_t GPS_TIME_PERIOD_MS = 1000;
static constexpr uint32_t GPS_TIME_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
    // End signal defs
} gps_time_data_t;

static constexpr uint32_t IGBT_TEMPS_MSG_ID = 0x4C;
static constexpr uint8_t  IGBT_TEMPS_DLC = 8;
static constexpr uint64_t IGBT_TEMPS_LAYOUT_HASH = 0x6E37809D0C79B94A;
static constexpr uint32_t IGBT_TEMPS_PERIOD_MS = 5000;
static constexpr uint32_t IGBT_TEMPS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t front_right;
    int16_t front_left;
    int16_t rear_left;
    int16_t rear_right;
    // End signal defs
} igbt_temps_data_t;

static constexpr uint32_t INVA_DIAGNOSTICS_MSG_ID = 0x4D;
static constexpr uint8_t  INVA_DIAGNOSTICS_DLC = 6;
static constexpr uint64_t INVA_DIAGNOSTICS_LAYOUT_HASH = 0xC9E38CBF49B07689;
static constexpr uint32_t INVA_DIAGNOSTICS_PERIOD_MS = 1000;
static constexpr uint32_t INVA_DIAGNOSTICS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t diagnostic_number;
    AMK_state_t amk_state;
    bool bError;
    bool InverterOn;
    bool is_any_stale;
    // End signal defs
} inva_diagnostics_data_t;

static constexpr uint32_t INVB_DIAGNOSTICS_MSG_ID = 0x4E;
static constexpr uint8_t  INVB_DIAGNOSTICS_DLC = 6;
static constexpr uint64_t INVB_DIAGNOSTICS_LAYOUT_HASH = 0xC9E38CBF49B07689;
static constexpr uint32_t INVB_DIAGNOSTICS_PERIOD_MS = 1000;
static constexpr uint32_t INVB_DIAGNOSTICS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t diagnostic_number;
    AMK_state_t amk_state;
    bool bError;
    bool InverterOn;
    bool is_any_stale;
    // End signal defs
} invb_diagnostics_data_t;

static constexpr uint32_t INVC_DIAGNOSTICS_MSG_ID = 0x4F;
static constexpr uint8_t  INVC_DIAGNOSTICS_DLC = 6;
static constexpr uint64_t INVC_DIAGNOSTICS_LAYOUT_HASH = 0xC9E38CBF49B07689;
static constexpr uint32_t INVC_DIAGNOSTICS_PERIOD_MS = 1000;
static constexpr uint32_t INVC_DIAGNOSTICS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t diagnostic_number;
    AMK_state_t amk_state;
    bool bError;
    bool InverterOn;
    bool is_any_stale;
    // End signal defs
} invc_diagnostics_data_t;

static constexpr uint32_t INVD_DIAGNOSTICS_MSG_ID = 0x50;
static constexpr uint8_t  INVD_DIAGNOSTICS_DLC = 6;
static constexpr uint64_t INVD_DIAGNOSTICS_LAYOUT_HASH = 0xC9E38CBF49B07689;
static constexpr uint32_t INVD_DIAGNOSTICS_PERIOD_MS = 1000;
static constexpr uint32_t INVD_DIAGNOSTICS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t diagnostic_number;
    AMK_state_t amk_state;
    bool bError;
    bool InverterOn;
    bool is_any_stale;
    // End signal defs
} invd_diagnostics_data_t;

static constexpr uint32_t MAIN_CAN_OVERFLOW_MSG_ID = 0x51;
static constexpr uint8_t  MAIN_CAN_OVERFLOW_DLC = 8;
static constexpr uint64_t MAIN_CAN_OVERFLOW_LAYOUT_HASH = 0x01BF3DAA664D6EE2;
static constexpr uint32_t MAIN_CAN_OVERFLOW_PERIOD_MS = 5000;
static constexpr uint32_t MAIN_CAN_OVERFLOW_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t rx_overflow;
    uint32_t tx_overflow;
    // End signal defs
} main_can_overflow_data_t;

static constexpr uint32_t MAIN_CAN_TX_ACTIONS_MSG_ID = 0x52;
static constexpr uint8_t  MAIN_CAN_TX_ACTIONS_DLC = 8;
static constexpr uint64_t MAIN_CAN_TX_ACTIONS_LAYOUT_HASH = 0xF833101CB26480B0;
static constexpr uint32_t MAIN_CAN_TX_ACTIONS_PERIOD_MS = 5000;
static constexpr uint32_t MAIN_CAN_TX_ACTIONS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t enqueue_count;
    uint32_t task_wake_count;
    // End signal defs
} main_can_tx_actions_data_t;

static constexpr uint32_t MAIN_CAN_TX_MESSAGES_MSG_ID = 0x53;
static constexpr uint8_t  MAIN_CAN_TX_MESSAGES_DLC = 8;
static constexpr uint64_t MAIN_CAN_TX_MESSAGES_LAYOUT_HASH = 0x1CEB661EBB6FA9BB;
static constexpr uint32_t MAIN_CAN_TX_MESSAGES_PERIOD_MS = 5000;
static constexpr uint32_t MAIN_CAN_TX_MESSAGES_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t tx_sent;
    uint32_t tx_callbacks;
    // End signal defs
} main_can_tx_messages_data_t;

static constexpr uint32_t MAIN_VERSION_MSG_ID = 0x54;
static constexpr uint8_t  MAIN_VERSION_DLC = 8;
static constexpr uint64_t MAIN_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t MAIN_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t MAIN_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} main_version_data_t;

static constexpr uint32_t MOTOR_TEMPS_MSG_ID = 0x55;
static constexpr uint8_t  MOTOR_TEMPS_DLC = 8;
static constexpr uint64_t MOTOR_TEMPS_LAYOUT_HASH = 0x6E37809D0C79B94A;
static constexpr uint32_t MOTOR_TEMPS_PERIOD_MS = 5000;
static constexpr uint32_t MOTOR_TEMPS_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t front_right;
    int16_t front_left;
    int16_t rear_left;
    int16_t rear_right;
    // End signal defs
} motor_temps_data_t;

static constexpr uint32_t PDU_VERSION_MSG_ID = 0x56;
static constexpr uint8_t  PDU_VERSION_DLC = 4;
static constexpr uint64_t PDU_VERSION_LAYOUT_HASH = 0x87B764C36D075A7A;
static constexpr uint32_t PDU_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t PDU_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    // End signal defs
} pdu_version_data_t;

static constexpr uint32_t TORQUE_VECTOR_VERSION_MSG_ID = 0x57;
static constexpr uint8_t  TORQUE_VECTOR_VERSION_DLC = 8;
static constexpr uint64_t TORQUE_VECTOR_VERSION_LAYOUT_HASH = 0xAAC3653F2D304C9B;
static constexpr uint32_t TORQUE_VECTOR_VERSION_PERIOD_MS = 5000;
static constexpr uint32_t TORQUE_VECTOR_VERSION_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t git_hash;
    uint32_t bootloader_git_hash;
    bool bootloadable;
    // End signal defs
} torque_vector_version_data_t;

static constexpr uint32_t BL_MAIN_MODULE_INFO_MSG_ID = 0x100;
static constexpr uint8_t  BL_MAIN_MODULE_INFO_DLC = 7;
static constexpr uint64_t BL_MAIN_MODULE_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_MAIN_MODULE_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_MAIN_MODULE_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_main_module_info_data_t;

static constexpr uint32_t BL_DASHBOARD_INFO_MSG_ID = 0x101;
static constexpr uint8_t  BL_DASHBOARD_INFO_DLC = 7;
static constexpr uint64_t BL_DASHBOARD_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_DASHBOARD_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_DASHBOARD_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_dashboard_info_data_t;

static constexpr uint32_t BL_TORQUE_VECTOR_INFO_MSG_ID = 0x102;
static constexpr uint8_t  BL_TORQUE_VECTOR_INFO_DLC = 7;
static constexpr uint64_t BL_TORQUE_VECTOR_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_TORQUE_VECTOR_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_TORQUE_VECTOR_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_torque_vector_info_data_t;

static constexpr uint32_t BL_A_BOX_INFO_MSG_ID = 0x103;
static constexpr uint8_t  BL_A_BOX_INFO_DLC = 7;
static constexpr uint64_t BL_A_BOX_INFO_LAYOUT_HASH = 0xF81229DEE49104EE;
static constexpr uint32_t BL_A_BOX_INFO_PERIOD_MS = 5000;
static constexpr uint32_t BL_A_BOX_INFO_STALE_TIMEOUT_MS = 12500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t protocol_version;
    uint32_t git_hash;
    bootloader_target_id_t target_id;
    uint8_t flags;
    // End signal defs
} bl_a_box_info_data_t;

static constexpr uint32_t MOTOR_SPEEDS_MSG_ID = 0x104;
static constexpr uint8_t  MOTOR_SPEEDS_DLC = 8;
static constexpr uint64_t MOTOR_SPEEDS_LAYOUT_HASH = 0x6E37809D0C79B94A;
static constexpr uint32_t MOTOR_SPEEDS_PERIOD_MS = 20;
static constexpr uint32_t MOTOR_SPEEDS_STALE_TIMEOUT_MS = 50;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t front_right;
    int16_t front_left;
    int16_t rear_left;
    int16_t rear_right;
    // End signal defs
} motor_speeds_data_t;

static constexpr uint32_t PACK_ANALOG_MSG_ID = 0x105;
static constexpr uint8_t  PACK_ANALOG_DLC = 4;
static constexpr uint64_t PACK_ANALOG_LAYOUT_HASH = 0x7B07C5DBAF5413C7;
static constexpr uint32_t PACK_ANALOG_PERIOD_MS = 10;
static constexpr uint32_t PACK_ANALOG_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t pack_voltage;
    int16_t pack_current;
    // End signal defs
} pack_analog_data_t;

static constexpr uint32_t VBATT_VMC_MSG_ID = 0x106;
static constexpr uint8_t  VBATT_VMC_DLC = 4;
static constexpr uint64_t VBATT_VMC_LAYOUT_HASH = 0x132601B41A81DD73;
static constexpr uint32_t VBATT_VMC_PERIOD_MS = 20;
static constexpr uint32_t VBATT_VMC_STALE_TIMEOUT_MS = 50;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t vbatt;
    uint16_t vmc;
    // End signal defs
} vbatt_vmc_data_t;

static constexpr uint32_t LWS_STANDARD_MSG_ID = 0x2B0;
static constexpr uint8_t  LWS_STANDARD_DLC = 5;
static constexpr uint64_t LWS_STANDARD_LAYOUT_HASH = 0x47E22F55CD3D223A;
static constexpr uint32_t LWS_STANDARD_PERIOD_MS = 10;
static constexpr uint32_t LWS_STANDARD_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t LWS_ANGLE;
    uint8_t LWS_SPEED;
    bool OK;
    bool CAL;
    bool TRIM;
    uint8_t reserved1;
    uint8_t reserved2;
    // End signal defs
} LWS_Standard_data_t;

static constexpr uint32_t IZZE_ANGULAR_RATE_MSG_ID = 0x4EC;
static constexpr uint8_t  IZZE_ANGULAR_RATE_DLC = 8;
static constexpr uint64_t IZZE_ANGULAR_RATE_LAYOUT_HASH = 0x1B1AAEFC1A933FFF;
static constexpr uint32_t IZZE_ANGULAR_RATE_PERIOD_MS = 50;
static constexpr uint32_t IZZE_ANGULAR_RATE_STALE_TIMEOUT_MS = 125;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t X_axis;
    int16_t Y_axis;
    int16_t Z_axis;
    uint16_t reserved;
    // End signal defs
} IZZE_angular_rate_data_t;

static constexpr uint32_t IZZE_ACCELERATION_MSG_ID = 0x4ED;
static constexpr uint8_t  IZZE_ACCELERATION_DLC = 8;
static constexpr uint64_t IZZE_ACCELERATION_LAYOUT_HASH = 0x8CC7E4AA8DA5EE11;
static constexpr uint32_t IZZE_ACCELERATION_PERIOD_MS = 50;
static constexpr uint32_t IZZE_ACCELERATION_STALE_TIMEOUT_MS = 125;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t X_axis;
    int16_t Y_axis;
    int16_t Z_axis;
    int16_t temperature;
    // End signal defs
} IZZE_acceleration_data_t;

static constexpr uint32_t LWS_CONFIG_MSG_ID = 0x7C0;
static constexpr uint8_t  LWS_CONFIG_DLC = 2;
static constexpr uint64_t LWS_CONFIG_LAYOUT_HASH = 0xF2EED4094FD9AF17;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint8_t CCW;
    uint8_t reserved1;
    uint8_t reserved2;
    // End signal defs
} LWS_Config_data_t;


#endif // VCAN_H
