#ifndef CCAN_H
#define CCAN_H

/**
 * @file CCAN.h
 * @brief Auto-generated CAN bus definitions for CCAN.
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

static constexpr uint32_t CCAN_BUS_ID = 2;
static constexpr PHAL_FDCAN_BaudRate_t CCAN_BAUD_RATE = FDCAN_BAUD_250K;

static constexpr uint32_t BMS_PECS_CCAN_MSG_ID = 0x800;
static constexpr uint8_t  BMS_PECS_CCAN_DLC = 2;
static constexpr uint64_t BMS_PECS_CCAN_LAYOUT_HASH = 0xA0950EA1443BECF5;
static constexpr uint32_t BMS_PECS_CCAN_PERIOD_MS = 200;
static constexpr uint32_t BMS_PECS_CCAN_STALE_TIMEOUT_MS = 500;

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
} bms_pecs_ccan_data_t;

static constexpr uint32_t BMS_VOLTAGES_CCAN_MSG_ID = 0x801;
static constexpr uint8_t  BMS_VOLTAGES_CCAN_DLC = 8;
static constexpr uint64_t BMS_VOLTAGES_CCAN_LAYOUT_HASH = 0x3B8969C6E5AD8494;
static constexpr uint32_t BMS_VOLTAGES_CCAN_PERIOD_MS = 200;
static constexpr uint32_t BMS_VOLTAGES_CCAN_STALE_TIMEOUT_MS = 500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t pack_voltage;
    uint16_t min_cell_voltage;
    uint16_t max_cell_voltage;
    uint16_t avg_cell_voltage;
    // End signal defs
} bms_voltages_ccan_data_t;

static constexpr uint32_t BMS_TEMPS_CCAN_MSG_ID = 0x802;
static constexpr uint8_t  BMS_TEMPS_CCAN_DLC = 6;
static constexpr uint64_t BMS_TEMPS_CCAN_LAYOUT_HASH = 0x4776F0D21CB8D88F;
static constexpr uint32_t BMS_TEMPS_CCAN_PERIOD_MS = 200;
static constexpr uint32_t BMS_TEMPS_CCAN_STALE_TIMEOUT_MS = 500;

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
} bms_temps_ccan_data_t;

static constexpr uint32_t CHARGE_REQUEST_MSG_ID = 0x803;
static constexpr uint8_t  CHARGE_REQUEST_DLC = 5;
static constexpr uint64_t CHARGE_REQUEST_LAYOUT_HASH = 0x5152F96953A6AAA1;
static constexpr uint32_t CHARGE_REQUEST_PERIOD_MS = 500;
static constexpr uint32_t CHARGE_REQUEST_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t charge_voltage;
    uint16_t charge_current;
    bool charge_enable;
    bool balance_enable;
    // End signal defs
} charge_request_data_t;

static constexpr uint32_t ELCON_COMMAND_MSG_ID = 0x1806E5F4;
static constexpr uint8_t  ELCON_COMMAND_DLC = 8;
static constexpr uint64_t ELCON_COMMAND_LAYOUT_HASH = 0xF50C0941ACE50635;
static constexpr uint32_t ELCON_COMMAND_PERIOD_MS = 1000;
static constexpr uint32_t ELCON_COMMAND_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t voltage_limit;
    uint16_t current_limit;
    uint8_t charger_disable;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    // End signal defs
} elcon_command_data_t;

static constexpr uint32_t ELCON_STATUS_MSG_ID = 0x18FF50E5;
static constexpr uint8_t  ELCON_STATUS_DLC = 5;
static constexpr uint64_t ELCON_STATUS_LAYOUT_HASH = 0x378D86F3A6539736;
static constexpr uint32_t ELCON_STATUS_PERIOD_MS = 1000;
static constexpr uint32_t ELCON_STATUS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t charge_voltage;
    uint16_t charge_current;
    bool hw_fail;
    bool temp_fail;
    bool input_v_fail;
    bool startup_fail;
    bool communication_fail;
    // End signal defs
} elcon_status_data_t;

static constexpr uint32_t CHARGING_FSM_INTERNALS_MSG_ID = 0x18FF50E6;
static constexpr uint8_t  CHARGING_FSM_INTERNALS_DLC = 6;
static constexpr uint64_t CHARGING_FSM_INTERNALS_LAYOUT_HASH = 0x9B9BF633E7F6501E;
static constexpr uint32_t CHARGING_FSM_INTERNALS_PERIOD_MS = 1000;
static constexpr uint32_t CHARGING_FSM_INTERNALS_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t voltage_limit;
    uint16_t current_limit;
    bool balancing_status;
    charging_state_t charging_state;
    // End signal defs
} charging_fsm_internals_data_t;

static constexpr uint32_t CELL_TELEMETRY_CCAN_MSG_ID = 0x18FF50E7;
static constexpr uint8_t  CELL_TELEMETRY_CCAN_DLC = 5;
static constexpr uint64_t CELL_TELEMETRY_CCAN_LAYOUT_HASH = 0xE1D5FC2D60B4D808;
static constexpr uint32_t CELL_TELEMETRY_CCAN_PERIOD_MS = 10;
static constexpr uint32_t CELL_TELEMETRY_CCAN_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t voltage;
    uint8_t module_num;
    uint8_t cell_num;
    bool balance_status;
    // End signal defs
} cell_telemetry_ccan_data_t;

static constexpr uint32_t PACK_ANALOG_CCAN_MSG_ID = 0x18FF50E8;
static constexpr uint8_t  PACK_ANALOG_CCAN_DLC = 4;
static constexpr uint64_t PACK_ANALOG_CCAN_LAYOUT_HASH = 0x7B07C5DBAF5413C7;
static constexpr uint32_t PACK_ANALOG_CCAN_PERIOD_MS = 10;
static constexpr uint32_t PACK_ANALOG_CCAN_STALE_TIMEOUT_MS = 25;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t pack_voltage;
    int16_t pack_current;
    // End signal defs
} pack_analog_ccan_data_t;

static constexpr uint32_t THERMISTOR_TELEMETRY_CCAN_MSG_ID = 0x18FF50E9;
static constexpr uint8_t  THERMISTOR_TELEMETRY_CCAN_DLC = 5;
static constexpr uint64_t THERMISTOR_TELEMETRY_CCAN_LAYOUT_HASH = 0x0E727A25C2834112;
static constexpr uint32_t THERMISTOR_TELEMETRY_CCAN_PERIOD_MS = 125;
static constexpr uint32_t THERMISTOR_TELEMETRY_CCAN_STALE_TIMEOUT_MS = 312;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t temperature;
    uint8_t module_num;
    uint8_t thermistor_num;
    bool is_board_temp;
    // End signal defs
} thermistor_telemetry_ccan_data_t;


#endif // CCAN_H
