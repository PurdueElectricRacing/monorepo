#ifndef A_BOX_H
#define A_BOX_H

/**
 * @file A_BOX.h
 * @brief Auto-generated CAN driver for A_BOX.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include <string.h>

#include "can_library/can_codec.h"
#include "can_library/can_common.h"
#include "can_library/generated/can_types.h"
#include "can_library/generated/can_version.h"
#include "can_library/generated/CCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_A_BOX uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN1
#define USE_FDCAN2
#define USE_FDCAN3

void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN3(CanMsgTypeDef_t *msg);

// Scaling Macros
// bms_voltages
static constexpr float PACK_COEFF_BMS_VOLTAGES_PACK_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_MIN_CELL_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_MAX_CELL_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_AVG_CELL_VOLTAGE = 100.0f;
// bms_voltages_ccan
static constexpr float PACK_COEFF_BMS_VOLTAGES_CCAN_PACK_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_CCAN_MIN_CELL_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_CCAN_MAX_CELL_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_BMS_VOLTAGES_CCAN_AVG_CELL_VOLTAGE = 100.0f;
// cell_telemetry
static constexpr float PACK_COEFF_CELL_TELEMETRY_VOLTAGE = 100.0f;
// cell_telemetry_ccan
static constexpr float PACK_COEFF_CELL_TELEMETRY_CCAN_VOLTAGE = 100.0f;
// charge_request
static constexpr float UNPACK_COEFF_CHARGE_REQUEST_CHARGE_VOLTAGE = 0.1f;
static constexpr float UNPACK_COEFF_CHARGE_REQUEST_CHARGE_CURRENT = 0.1f;
// charging_fsm_internals
static constexpr float PACK_COEFF_CHARGING_FSM_INTERNALS_VOLTAGE_LIMIT = 10.0f;
static constexpr float PACK_COEFF_CHARGING_FSM_INTERNALS_CURRENT_LIMIT = 10.0f;
// e_meter_temperature
static constexpr float PACK_COEFF_E_METER_TEMPERATURE_MAX_CELL_TEMP = 10.0f;
static constexpr float PACK_COEFF_E_METER_TEMPERATURE_MIN_CELL_TEMP = 10.0f;
static constexpr float PACK_COEFF_E_METER_TEMPERATURE_AVG_CELL_TEMP = 10.0f;
// elcon_command
static constexpr float PACK_COEFF_ELCON_COMMAND_VOLTAGE_LIMIT = 10.0f;
static constexpr float PACK_COEFF_ELCON_COMMAND_CURRENT_LIMIT = 10.0f;
// elcon_status
static constexpr float UNPACK_COEFF_ELCON_STATUS_CHARGE_VOLTAGE = 0.1f;
static constexpr float UNPACK_COEFF_ELCON_STATUS_CHARGE_CURRENT = 0.1f;
// pack_analog
static constexpr float PACK_COEFF_PACK_ANALOG_PACK_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_PACK_ANALOG_PACK_CURRENT = 10.0f;
// pack_analog_ccan
static constexpr float PACK_COEFF_PACK_ANALOG_CCAN_PACK_VOLTAGE = 100.0f;
static constexpr float PACK_COEFF_PACK_ANALOG_CCAN_PACK_CURRENT = 10.0f;
// thermistor_telemetry
static constexpr float PACK_COEFF_THERMISTOR_TELEMETRY_TEMPERATURE = 10.0f;
// thermistor_telemetry_ccan
static constexpr float PACK_COEFF_THERMISTOR_TELEMETRY_CCAN_TEMPERATURE = 10.0f;

// Offset Macros

// Data Structures
typedef struct {
    elcon_status_data_t elcon_status;
    charge_request_data_t charge_request;
    bl_a_box_start_data_t bl_a_box_start;
    dashboard_fault_event_data_t dashboard_fault_event;
    main_module_fault_event_data_t main_module_fault_event;
    pdu_fault_event_data_t pdu_fault_event;
    torque_vector_fault_event_data_t torque_vector_fault_event;
    dashboard_fault_sync_data_t dashboard_fault_sync;
    main_module_fault_sync_data_t main_module_fault_sync;
    pdu_fault_sync_data_t pdu_fault_sync;
    torque_vector_fault_sync_data_t torque_vector_fault_sync;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void bl_a_box_start_CALLBACK();
extern void dashboard_fault_event_CALLBACK();
extern void main_module_fault_event_CALLBACK();
extern void pdu_fault_event_CALLBACK();
extern void torque_vector_fault_event_CALLBACK();
extern void dashboard_fault_sync_CALLBACK();
extern void main_module_fault_sync_CALLBACK();
extern void pdu_fault_sync_CALLBACK();
extern void torque_vector_fault_sync_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN1) {
        switch (id) {
            case BL_A_BOX_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_a_box_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_a_box_start.last_rx = xTaskGetTickCount();
                bl_a_box_start_CALLBACK();
                break;
            }
            case DASHBOARD_FAULT_EVENT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_idx = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.dashboard_fault_event.idx = (fault_id_t)sig_raw_idx;
                uint64_t sig_raw_val = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.dashboard_fault_event.val = (uint16_t)sig_raw_val;
                uint64_t sig_raw_state = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.dashboard_fault_event.state = (bool)sig_raw_state;

                can_data.dashboard_fault_event.last_rx = xTaskGetTickCount();
                dashboard_fault_event_CALLBACK();
                break;
            }
            case MAIN_MODULE_FAULT_EVENT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_idx = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.main_module_fault_event.idx = (fault_id_t)sig_raw_idx;
                uint64_t sig_raw_val = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.main_module_fault_event.val = (uint16_t)sig_raw_val;
                uint64_t sig_raw_state = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.main_module_fault_event.state = (bool)sig_raw_state;

                can_data.main_module_fault_event.last_rx = xTaskGetTickCount();
                main_module_fault_event_CALLBACK();
                break;
            }
            case PDU_FAULT_EVENT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_idx = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.pdu_fault_event.idx = (fault_id_t)sig_raw_idx;
                uint64_t sig_raw_val = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.pdu_fault_event.val = (uint16_t)sig_raw_val;
                uint64_t sig_raw_state = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.pdu_fault_event.state = (bool)sig_raw_state;

                can_data.pdu_fault_event.last_rx = xTaskGetTickCount();
                pdu_fault_event_CALLBACK();
                break;
            }
            case TORQUE_VECTOR_FAULT_EVENT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_idx = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.torque_vector_fault_event.idx = (fault_id_t)sig_raw_idx;
                uint64_t sig_raw_val = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.torque_vector_fault_event.val = (uint16_t)sig_raw_val;
                uint64_t sig_raw_state = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.torque_vector_fault_event.state = (bool)sig_raw_state;

                can_data.torque_vector_fault_event.last_rx = xTaskGetTickCount();
                torque_vector_fault_event_CALLBACK();
                break;
            }
            case DASHBOARD_FAULT_SYNC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_APPS_wiring_t1 = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.dashboard_fault_sync.APPS_wiring_t1 = (bool)sig_raw_APPS_wiring_t1;
                uint64_t sig_raw_APPS_wiring_t2 = CAN_unpack_raw_signal(host_data, 0x1, 1, BSWAP_NONE);
                can_data.dashboard_fault_sync.APPS_wiring_t2 = (bool)sig_raw_APPS_wiring_t2;
                uint64_t sig_raw_APPS_IMPLAUSIBLE = CAN_unpack_raw_signal(host_data, 0x1, 2, BSWAP_NONE);
                can_data.dashboard_fault_sync.APPS_IMPLAUSIBLE = (bool)sig_raw_APPS_IMPLAUSIBLE;
                uint64_t sig_raw_APPS_BRAKE = CAN_unpack_raw_signal(host_data, 0x1, 3, BSWAP_NONE);
                can_data.dashboard_fault_sync.APPS_BRAKE = (bool)sig_raw_APPS_BRAKE;
                uint64_t sig_raw_BSE = CAN_unpack_raw_signal(host_data, 0x1, 4, BSWAP_NONE);
                can_data.dashboard_fault_sync.BSE = (bool)sig_raw_BSE;

                can_data.dashboard_fault_sync.last_rx = xTaskGetTickCount();
                dashboard_fault_sync_CALLBACK();
                break;
            }
            case MAIN_MODULE_FAULT_SYNC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_SDC1_IMD = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC1_IMD = (bool)sig_raw_SDC1_IMD;
                uint64_t sig_raw_SDC2_BMS = CAN_unpack_raw_signal(host_data, 0x1, 1, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC2_BMS = (bool)sig_raw_SDC2_BMS;
                uint64_t sig_raw_SDC3_BSPD = CAN_unpack_raw_signal(host_data, 0x1, 2, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC3_BSPD = (bool)sig_raw_SDC3_BSPD;
                uint64_t sig_raw_SDC4_ECU_OK = CAN_unpack_raw_signal(host_data, 0x1, 3, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC4_ECU_OK = (bool)sig_raw_SDC4_ECU_OK;
                uint64_t sig_raw_SDC5_BOTS = CAN_unpack_raw_signal(host_data, 0x1, 4, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC5_BOTS = (bool)sig_raw_SDC5_BOTS;
                uint64_t sig_raw_SDC6_INERTIA = CAN_unpack_raw_signal(host_data, 0x1, 5, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC6_INERTIA = (bool)sig_raw_SDC6_INERTIA;
                uint64_t sig_raw_SDC7_COCKPIT_ESTOP = CAN_unpack_raw_signal(host_data, 0x1, 6, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC7_COCKPIT_ESTOP = (bool)sig_raw_SDC7_COCKPIT_ESTOP;
                uint64_t sig_raw_SDC9_FRONT_INTERLOCK = CAN_unpack_raw_signal(host_data, 0x1, 7, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC9_FRONT_INTERLOCK = (bool)sig_raw_SDC9_FRONT_INTERLOCK;
                uint64_t sig_raw_SDC10_RIGHT_ESTOP = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC10_RIGHT_ESTOP = (bool)sig_raw_SDC10_RIGHT_ESTOP;
                uint64_t sig_raw_SDC11_LEFT_ESTOP = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC11_LEFT_ESTOP = (bool)sig_raw_SDC11_LEFT_ESTOP;
                uint64_t sig_raw_SDC12_MSD = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC12_MSD = (bool)sig_raw_SDC12_MSD;
                uint64_t sig_raw_SDC13_E_METER = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC13_E_METER = (bool)sig_raw_SDC13_E_METER;
                uint64_t sig_raw_SDC14_HVD = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC14_HVD = (bool)sig_raw_SDC14_HVD;
                uint64_t sig_raw_SDC16_REAR_INTERLOCK = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC16_REAR_INTERLOCK = (bool)sig_raw_SDC16_REAR_INTERLOCK;
                uint64_t sig_raw_SDC17_TSMS = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC17_TSMS = (bool)sig_raw_SDC17_TSMS;
                uint64_t sig_raw_SDC19_PRECHARGE_COMPLETE = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.main_module_fault_sync.SDC19_PRECHARGE_COMPLETE = (bool)sig_raw_SDC19_PRECHARGE_COMPLETE;
                uint64_t sig_raw_PRECHARGE_INCOMPLETE = CAN_unpack_raw_signal(host_data, 0x1, 16, BSWAP_NONE);
                can_data.main_module_fault_sync.PRECHARGE_INCOMPLETE = (bool)sig_raw_PRECHARGE_INCOMPLETE;

                can_data.main_module_fault_sync.last_rx = xTaskGetTickCount();
                main_module_fault_sync_CALLBACK();
                break;
            }
            case PDU_FAULT_SYNC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_SDC_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.pdu_fault_sync.SDC_SWITCH_FAULT = (bool)sig_raw_SDC_SWITCH_FAULT;
                uint64_t sig_raw_SDC_LOOP_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 1, BSWAP_NONE);
                can_data.pdu_fault_sync.SDC_LOOP_SWITCH_FAULT = (bool)sig_raw_SDC_LOOP_SWITCH_FAULT;
                uint64_t sig_raw_MAIN_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 2, BSWAP_NONE);
                can_data.pdu_fault_sync.MAIN_SWITCH_FAULT = (bool)sig_raw_MAIN_SWITCH_FAULT;
                uint64_t sig_raw_DASH_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 3, BSWAP_NONE);
                can_data.pdu_fault_sync.DASH_SWITCH_FAULT = (bool)sig_raw_DASH_SWITCH_FAULT;
                uint64_t sig_raw_DAQ_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 4, BSWAP_NONE);
                can_data.pdu_fault_sync.DAQ_SWITCH_FAULT = (bool)sig_raw_DAQ_SWITCH_FAULT;
                uint64_t sig_raw_ABOX_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 5, BSWAP_NONE);
                can_data.pdu_fault_sync.ABOX_SWITCH_FAULT = (bool)sig_raw_ABOX_SWITCH_FAULT;
                uint64_t sig_raw_HV_BOARD_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 6, BSWAP_NONE);
                can_data.pdu_fault_sync.HV_BOARD_SWITCH_FAULT = (bool)sig_raw_HV_BOARD_SWITCH_FAULT;
                uint64_t sig_raw_TV_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 7, BSWAP_NONE);
                can_data.pdu_fault_sync.TV_SWITCH_FAULT = (bool)sig_raw_TV_SWITCH_FAULT;
                uint64_t sig_raw_DL_FRONT_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.pdu_fault_sync.DL_FRONT_SWITCH_FAULT = (bool)sig_raw_DL_FRONT_SWITCH_FAULT;
                uint64_t sig_raw_DL_REAR_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.pdu_fault_sync.DL_REAR_SWITCH_FAULT = (bool)sig_raw_DL_REAR_SWITCH_FAULT;
                uint64_t sig_raw_AMK1_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.pdu_fault_sync.AMK1_SWITCH_FAULT = (bool)sig_raw_AMK1_SWITCH_FAULT;
                uint64_t sig_raw_AMK2_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.pdu_fault_sync.AMK2_SWITCH_FAULT = (bool)sig_raw_AMK2_SWITCH_FAULT;
                uint64_t sig_raw_PUMP1_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.pdu_fault_sync.PUMP1_SWITCH_FAULT = (bool)sig_raw_PUMP1_SWITCH_FAULT;
                uint64_t sig_raw_PUMP2_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.pdu_fault_sync.PUMP2_SWITCH_FAULT = (bool)sig_raw_PUMP2_SWITCH_FAULT;
                uint64_t sig_raw_HXFAN_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.pdu_fault_sync.HXFAN_SWITCH_FAULT = (bool)sig_raw_HXFAN_SWITCH_FAULT;
                uint64_t sig_raw_FAN1_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.pdu_fault_sync.FAN1_SWITCH_FAULT = (bool)sig_raw_FAN1_SWITCH_FAULT;
                uint64_t sig_raw_FAN2_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 16, BSWAP_NONE);
                can_data.pdu_fault_sync.FAN2_SWITCH_FAULT = (bool)sig_raw_FAN2_SWITCH_FAULT;
                uint64_t sig_raw_FAN3_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 17, BSWAP_NONE);
                can_data.pdu_fault_sync.FAN3_SWITCH_FAULT = (bool)sig_raw_FAN3_SWITCH_FAULT;
                uint64_t sig_raw_FAN4_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 18, BSWAP_NONE);
                can_data.pdu_fault_sync.FAN4_SWITCH_FAULT = (bool)sig_raw_FAN4_SWITCH_FAULT;
                uint64_t sig_raw_FAN5_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 19, BSWAP_NONE);
                can_data.pdu_fault_sync.FAN5_SWITCH_FAULT = (bool)sig_raw_FAN5_SWITCH_FAULT;
                uint64_t sig_raw_EMETER_SWITCH_FAULT = CAN_unpack_raw_signal(host_data, 0x1, 20, BSWAP_NONE);
                can_data.pdu_fault_sync.EMETER_SWITCH_FAULT = (bool)sig_raw_EMETER_SWITCH_FAULT;

                can_data.pdu_fault_sync.last_rx = xTaskGetTickCount();
                pdu_fault_sync_CALLBACK();
                break;
            }
            case TORQUE_VECTOR_FAULT_SYNC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_GPS_INVALID_FIX = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.torque_vector_fault_sync.GPS_INVALID_FIX = (bool)sig_raw_GPS_INVALID_FIX;
                uint64_t sig_raw_GPS_WEAK_FIX = CAN_unpack_raw_signal(host_data, 0x1, 1, BSWAP_NONE);
                can_data.torque_vector_fault_sync.GPS_WEAK_FIX = (bool)sig_raw_GPS_WEAK_FIX;
                uint64_t sig_raw_GPS_INVALID_UTC = CAN_unpack_raw_signal(host_data, 0x1, 2, BSWAP_NONE);
                can_data.torque_vector_fault_sync.GPS_INVALID_UTC = (bool)sig_raw_GPS_INVALID_UTC;

                can_data.torque_vector_fault_sync.last_rx = xTaskGetTickCount();
                torque_vector_fault_sync_CALLBACK();
                break;
            }
            default:
                break;
        }
    }
    else if (bus == FDCAN2) {
        switch (id) {
            case ELCON_STATUS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_charge_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_16);
                can_data.elcon_status.charge_voltage = (uint16_t)sig_raw_charge_voltage;
                uint64_t sig_raw_charge_current = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_16);
                can_data.elcon_status.charge_current = (uint16_t)sig_raw_charge_current;
                uint64_t sig_raw_hw_fail = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.elcon_status.hw_fail = (bool)sig_raw_hw_fail;
                uint64_t sig_raw_temp_fail = CAN_unpack_raw_signal(host_data, 0x1, 33, BSWAP_NONE);
                can_data.elcon_status.temp_fail = (bool)sig_raw_temp_fail;
                uint64_t sig_raw_input_v_fail = CAN_unpack_raw_signal(host_data, 0x1, 34, BSWAP_NONE);
                can_data.elcon_status.input_v_fail = (bool)sig_raw_input_v_fail;
                uint64_t sig_raw_startup_fail = CAN_unpack_raw_signal(host_data, 0x1, 35, BSWAP_NONE);
                can_data.elcon_status.startup_fail = (bool)sig_raw_startup_fail;
                uint64_t sig_raw_communication_fail = CAN_unpack_raw_signal(host_data, 0x1, 36, BSWAP_NONE);
                can_data.elcon_status.communication_fail = (bool)sig_raw_communication_fail;

                can_data.elcon_status.last_rx = xTaskGetTickCount();
                break;
            }
            case CHARGE_REQUEST_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_charge_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.charge_request.charge_voltage = (uint16_t)sig_raw_charge_voltage;
                uint64_t sig_raw_charge_current = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.charge_request.charge_current = (uint16_t)sig_raw_charge_current;
                uint64_t sig_raw_charge_enable = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.charge_request.charge_enable = (bool)sig_raw_charge_enable;
                uint64_t sig_raw_balance_enable = CAN_unpack_raw_signal(host_data, 0x1, 33, BSWAP_NONE);
                can_data.charge_request.balance_enable = (bool)sig_raw_balance_enable;

                can_data.charge_request.last_rx = xTaskGetTickCount();
                break;
            }
            default:
                break;
        }
    }
}

// Stale Checking
static bool is_elcon_status_stale() {
    return (xTaskGetTickCount() - can_data.elcon_status.last_rx) > ELCON_STATUS_STALE_TIMEOUT_MS;
}
static bool is_charge_request_stale() {
    return (xTaskGetTickCount() - can_data.charge_request.last_rx) > CHARGE_REQUEST_STALE_TIMEOUT_MS;
}
static bool is_dashboard_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.dashboard_fault_sync.last_rx) > DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS;
}
static bool is_main_module_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.main_module_fault_sync.last_rx) > MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS;
}
static bool is_pdu_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.pdu_fault_sync.last_rx) > PDU_FAULT_SYNC_STALE_TIMEOUT_MS;
}
static bool is_torque_vector_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.torque_vector_fault_sync.last_rx) > TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS;
}

// Data initialization
static inline void CAN_init_data(void) {
    memset((void *)&can_data, 0, sizeof(can_data));
    // Underflow the last_rx timers to ensure messages are stale until first rx
    // Setup the stale function pointers
    can_data.elcon_status.last_rx = (uint32_t) -(ELCON_STATUS_STALE_TIMEOUT_MS + 1);
    can_data.elcon_status.is_stale = is_elcon_status_stale;
    can_data.charge_request.last_rx = (uint32_t) -(CHARGE_REQUEST_STALE_TIMEOUT_MS + 1);
    can_data.charge_request.is_stale = is_charge_request_stale;
    can_data.dashboard_fault_sync.last_rx = (uint32_t) -(DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.dashboard_fault_sync.is_stale = is_dashboard_fault_sync_stale;
    can_data.main_module_fault_sync.last_rx = (uint32_t) -(MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.main_module_fault_sync.is_stale = is_main_module_fault_sync_stale;
    can_data.pdu_fault_sync.last_rx = (uint32_t) -(PDU_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.pdu_fault_sync.is_stale = is_pdu_fault_sync_stale;
    can_data.torque_vector_fault_sync.last_rx = (uint32_t) -(TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.torque_vector_fault_sync.is_stale = is_torque_vector_fault_sync_stale;
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_elcon_command(
    uint16_t value_voltage_limit,
    uint16_t value_current_limit,
    uint8_t value_charger_disable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = ELCON_COMMAND_MSG_ID,
        .IDE=true,
        .DLC = ELCON_COMMAND_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_voltage_limit = (uint64_t)value_voltage_limit;
    payload = CAN_pack_raw_signal(payload, sig_raw_voltage_limit, 0xFFFF, 0, BSWAP_16);
    uint64_t sig_raw_current_limit = (uint64_t)value_current_limit;
    payload = CAN_pack_raw_signal(payload, sig_raw_current_limit, 0xFFFF, 16, BSWAP_16);
    uint64_t sig_raw_charger_disable = (uint64_t)value_charger_disable;
    payload = CAN_pack_raw_signal(payload, sig_raw_charger_disable, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_reserved1 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved1, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_reserved2 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved2, 0xFF, 48, BSWAP_NONE);
    uint64_t sig_raw_reserved3 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved3, 0xFF, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, ELCON_COMMAND_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_charging_fsm_internals(
    uint16_t value_voltage_limit,
    uint16_t value_current_limit,
    bool value_balancing_status,
    charging_state_t value_charging_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = CHARGING_FSM_INTERNALS_MSG_ID,
        .IDE=true,
        .DLC = CHARGING_FSM_INTERNALS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_voltage_limit = (uint64_t)value_voltage_limit;
    payload = CAN_pack_raw_signal(payload, sig_raw_voltage_limit, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_current_limit = (uint64_t)value_current_limit;
    payload = CAN_pack_raw_signal(payload, sig_raw_current_limit, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_balancing_status = (uint64_t)value_balancing_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_balancing_status, 0x1, 32, BSWAP_NONE);
    uint64_t sig_raw_charging_state = (uint64_t)value_charging_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_charging_state, 0xFF, 33, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, CHARGING_FSM_INTERNALS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pack_analog_ccan(
    uint16_t value_pack_voltage,
    int16_t value_pack_current
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = PACK_ANALOG_CCAN_MSG_ID,
        .IDE=true,
        .DLC = PACK_ANALOG_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_pack_voltage = (uint64_t)value_pack_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_pack_current = (uint64_t)value_pack_current;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_current, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PACK_ANALOG_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_voltages_ccan(
    uint16_t value_pack_voltage,
    uint16_t value_min_cell_voltage,
    uint16_t value_max_cell_voltage,
    uint16_t value_avg_cell_voltage
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = BMS_VOLTAGES_CCAN_MSG_ID,
        .IDE=true,
        .DLC = BMS_VOLTAGES_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_pack_voltage = (uint64_t)value_pack_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_min_cell_voltage = (uint64_t)value_min_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_cell_voltage, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_max_cell_voltage = (uint64_t)value_max_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_cell_voltage, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_avg_cell_voltage = (uint64_t)value_avg_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_cell_voltage, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_VOLTAGES_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_temps_ccan(
    uint8_t value_min_cell_temp,
    uint8_t value_max_cell_temp,
    uint8_t value_avg_cell_temp,
    uint8_t value_min_board_temp,
    uint8_t value_max_board_temp,
    uint8_t value_avg_board_temp
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = BMS_TEMPS_CCAN_MSG_ID,
        .IDE=true,
        .DLC = BMS_TEMPS_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_min_cell_temp = (uint64_t)value_min_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_cell_temp, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_max_cell_temp = (uint64_t)value_max_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_cell_temp, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_avg_cell_temp = (uint64_t)value_avg_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_cell_temp, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_min_board_temp = (uint64_t)value_min_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_board_temp, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_max_board_temp = (uint64_t)value_max_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_board_temp, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_avg_board_temp = (uint64_t)value_avg_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_board_temp, 0xFF, 40, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_TEMPS_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_pecs_ccan(
    bool value_RDCV_0_PEC,
    bool value_RDCV_1_PEC,
    bool value_RDCV_2_PEC,
    bool value_RDCV_3_PEC,
    bool value_RDCV_4_PEC,
    bool value_RDCV_5_PEC,
    bool value_RDAUX_0_PEC,
    bool value_RDAUX_1_PEC,
    bool value_RDAUX_2_PEC,
    bool value_RDAUX_3_PEC
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = BMS_PECS_CCAN_MSG_ID,
        .IDE=true,
        .DLC = BMS_PECS_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_RDCV_0_PEC = (uint64_t)value_RDCV_0_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_0_PEC, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_RDCV_1_PEC = (uint64_t)value_RDCV_1_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_1_PEC, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_RDCV_2_PEC = (uint64_t)value_RDCV_2_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_2_PEC, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_RDCV_3_PEC = (uint64_t)value_RDCV_3_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_3_PEC, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_RDCV_4_PEC = (uint64_t)value_RDCV_4_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_4_PEC, 0x1, 4, BSWAP_NONE);
    uint64_t sig_raw_RDCV_5_PEC = (uint64_t)value_RDCV_5_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_5_PEC, 0x1, 5, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_0_PEC = (uint64_t)value_RDAUX_0_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_0_PEC, 0x1, 6, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_1_PEC = (uint64_t)value_RDAUX_1_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_1_PEC, 0x1, 7, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_2_PEC = (uint64_t)value_RDAUX_2_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_2_PEC, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_3_PEC = (uint64_t)value_RDAUX_3_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_3_PEC, 0x1, 9, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_PECS_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_cell_telemetry_ccan(
    uint16_t value_voltage,
    uint8_t value_module_num,
    uint8_t value_cell_num,
    bool value_balance_status
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = CELL_TELEMETRY_CCAN_MSG_ID,
        .IDE=true,
        .DLC = CELL_TELEMETRY_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_voltage = (uint64_t)value_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_module_num = (uint64_t)value_module_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_module_num, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_cell_num = (uint64_t)value_cell_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_cell_num, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_balance_status = (uint64_t)value_balance_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_balance_status, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, CELL_TELEMETRY_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_thermistor_telemetry_ccan(
    uint16_t value_temperature,
    uint8_t value_module_num,
    uint8_t value_thermistor_num,
    bool value_is_board_temp
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .ExtId = THERMISTOR_TELEMETRY_CCAN_MSG_ID,
        .IDE=true,
        .DLC = THERMISTOR_TELEMETRY_CCAN_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_temperature = (uint64_t)value_temperature;
    payload = CAN_pack_raw_signal(payload, sig_raw_temperature, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_module_num = (uint64_t)value_module_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_module_num, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_thermistor_num = (uint64_t)value_thermistor_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_thermistor_num, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_is_board_temp = (uint64_t)value_is_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_board_temp, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, THERMISTOR_TELEMETRY_CCAN_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_pecs(
    bool value_RDCV_0_PEC,
    bool value_RDCV_1_PEC,
    bool value_RDCV_2_PEC,
    bool value_RDCV_3_PEC,
    bool value_RDCV_4_PEC,
    bool value_RDCV_5_PEC,
    bool value_RDAUX_0_PEC,
    bool value_RDAUX_1_PEC,
    bool value_RDAUX_2_PEC,
    bool value_RDAUX_3_PEC
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = BMS_PECS_MSG_ID,
        .IDE = false,
        .DLC = BMS_PECS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_RDCV_0_PEC = (uint64_t)value_RDCV_0_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_0_PEC, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_RDCV_1_PEC = (uint64_t)value_RDCV_1_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_1_PEC, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_RDCV_2_PEC = (uint64_t)value_RDCV_2_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_2_PEC, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_RDCV_3_PEC = (uint64_t)value_RDCV_3_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_3_PEC, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_RDCV_4_PEC = (uint64_t)value_RDCV_4_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_4_PEC, 0x1, 4, BSWAP_NONE);
    uint64_t sig_raw_RDCV_5_PEC = (uint64_t)value_RDCV_5_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDCV_5_PEC, 0x1, 5, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_0_PEC = (uint64_t)value_RDAUX_0_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_0_PEC, 0x1, 6, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_1_PEC = (uint64_t)value_RDAUX_1_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_1_PEC, 0x1, 7, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_2_PEC = (uint64_t)value_RDAUX_2_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_2_PEC, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_RDAUX_3_PEC = (uint64_t)value_RDAUX_3_PEC;
    payload = CAN_pack_raw_signal(payload, sig_raw_RDAUX_3_PEC, 0x1, 9, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_PECS_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_cell_telemetry(
    uint16_t value_voltage,
    uint8_t value_module_num,
    uint8_t value_cell_num,
    bool value_balance_status
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = CELL_TELEMETRY_MSG_ID,
        .IDE = false,
        .DLC = CELL_TELEMETRY_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_voltage = (uint64_t)value_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_module_num = (uint64_t)value_module_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_module_num, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_cell_num = (uint64_t)value_cell_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_cell_num, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_balance_status = (uint64_t)value_balance_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_balance_status, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, CELL_TELEMETRY_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_thermistor_telemetry(
    uint16_t value_temperature,
    uint8_t value_module_num,
    uint8_t value_thermistor_num,
    bool value_is_board_temp
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = THERMISTOR_TELEMETRY_MSG_ID,
        .IDE = false,
        .DLC = THERMISTOR_TELEMETRY_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_temperature = (uint64_t)value_temperature;
    payload = CAN_pack_raw_signal(payload, sig_raw_temperature, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_module_num = (uint64_t)value_module_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_module_num, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_thermistor_num = (uint64_t)value_thermistor_num;
    payload = CAN_pack_raw_signal(payload, sig_raw_thermistor_num, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_is_board_temp = (uint64_t)value_is_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_board_temp, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, THERMISTOR_TELEMETRY_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_e_meter_temperature(
    uint16_t value_max_cell_temp,
    uint16_t value_min_cell_temp,
    uint16_t value_avg_cell_temp
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = E_METER_TEMPERATURE_MSG_ID,
        .IDE = false,
        .DLC = E_METER_TEMPERATURE_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_max_cell_temp = (uint64_t)value_max_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_cell_temp, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_min_cell_temp = (uint64_t)value_min_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_cell_temp, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_avg_cell_temp = (uint64_t)value_avg_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_cell_temp, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_reserved1 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved1, 0xFF, 48, BSWAP_NONE);
    uint64_t sig_raw_reserved2 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved2, 0xFF, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, E_METER_TEMPERATURE_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_imd_state(
    imd_state_t value_imd_state,
    bool value_imd_status,
    imd_r_iso_quality_t value_imd_r_iso_quality,
    uint16_t value_r_iso
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = IMD_STATE_MSG_ID,
        .IDE = false,
        .DLC = IMD_STATE_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_imd_state = (uint64_t)value_imd_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_imd_state, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_imd_status = (uint64_t)value_imd_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_imd_status, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_imd_r_iso_quality = (uint64_t)value_imd_r_iso_quality;
    payload = CAN_pack_raw_signal(payload, sig_raw_imd_r_iso_quality, 0xFF, 9, BSWAP_NONE);
    uint64_t sig_raw_r_iso = (uint64_t)value_r_iso;
    payload = CAN_pack_raw_signal(payload, sig_raw_r_iso, 0xFFFF, 17, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IMD_STATE_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_a_box_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = A_BOX_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = A_BOX_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, A_BOX_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_a_box_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = A_BOX_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = A_BOX_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, A_BOX_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_a_box_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = A_BOX_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = A_BOX_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, A_BOX_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_abox_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = ABOX_INIT_MSG_ID,
        .IDE = false,
        .DLC = ABOX_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, ABOX_INIT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pack_analog(
    uint16_t value_pack_voltage,
    int16_t value_pack_current
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = PACK_ANALOG_MSG_ID,
        .IDE = false,
        .DLC = PACK_ANALOG_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_pack_voltage = (uint64_t)value_pack_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_pack_current = (uint64_t)value_pack_current;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_current, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PACK_ANALOG_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_voltages(
    uint16_t value_pack_voltage,
    uint16_t value_min_cell_voltage,
    uint16_t value_max_cell_voltage,
    uint16_t value_avg_cell_voltage
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = BMS_VOLTAGES_MSG_ID,
        .IDE = false,
        .DLC = BMS_VOLTAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_pack_voltage = (uint64_t)value_pack_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_pack_voltage, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_min_cell_voltage = (uint64_t)value_min_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_cell_voltage, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_max_cell_voltage = (uint64_t)value_max_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_cell_voltage, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_avg_cell_voltage = (uint64_t)value_avg_cell_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_cell_voltage, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_VOLTAGES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_bms_temps(
    uint8_t value_min_cell_temp,
    uint8_t value_max_cell_temp,
    uint8_t value_avg_cell_temp,
    uint8_t value_min_board_temp,
    uint8_t value_max_board_temp,
    uint8_t value_avg_board_temp
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = BMS_TEMPS_MSG_ID,
        .IDE = false,
        .DLC = BMS_TEMPS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_min_cell_temp = (uint64_t)value_min_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_cell_temp, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_max_cell_temp = (uint64_t)value_max_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_cell_temp, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_avg_cell_temp = (uint64_t)value_avg_cell_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_cell_temp, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_min_board_temp = (uint64_t)value_min_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_min_board_temp, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_max_board_temp = (uint64_t)value_max_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_max_board_temp, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_avg_board_temp = (uint64_t)value_avg_board_temp;
    payload = CAN_pack_raw_signal(payload, sig_raw_avg_board_temp, 0xFF, 40, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BMS_TEMPS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_abox_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = ABOX_VERSION_MSG_ID,
        .IDE = false,
        .DLC = ABOX_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, ABOX_VERSION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_a_box_fault_event(
    fault_id_t value_idx,
    uint16_t value_val,
    bool value_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = A_BOX_FAULT_EVENT_MSG_ID,
        .IDE = false,
        .DLC = A_BOX_FAULT_EVENT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_idx = (uint64_t)value_idx;
    payload = CAN_pack_raw_signal(payload, sig_raw_idx, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_val = (uint64_t)value_val;
    payload = CAN_pack_raw_signal(payload, sig_raw_val, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_state = (uint64_t)value_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_state, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, A_BOX_FAULT_EVENT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_a_box_fault_sync(
    bool value_PACK_OVERTEMP,
    bool value_PACK_WARM,
    bool value_PACK_COLD,
    bool value_IMD,
    bool value_BMS_DISCONNECTED,
    bool value_CELL_UNDERVOLTAGE,
    bool value_CELL_OVERVOLTAGE,
    bool value_PACK_FULL,
    bool value_PACK_EMPTY
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = A_BOX_FAULT_SYNC_MSG_ID,
        .IDE = false,
        .DLC = A_BOX_FAULT_SYNC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_PACK_OVERTEMP = (uint64_t)value_PACK_OVERTEMP;
    payload = CAN_pack_raw_signal(payload, sig_raw_PACK_OVERTEMP, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_PACK_WARM = (uint64_t)value_PACK_WARM;
    payload = CAN_pack_raw_signal(payload, sig_raw_PACK_WARM, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_PACK_COLD = (uint64_t)value_PACK_COLD;
    payload = CAN_pack_raw_signal(payload, sig_raw_PACK_COLD, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_IMD = (uint64_t)value_IMD;
    payload = CAN_pack_raw_signal(payload, sig_raw_IMD, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_BMS_DISCONNECTED = (uint64_t)value_BMS_DISCONNECTED;
    payload = CAN_pack_raw_signal(payload, sig_raw_BMS_DISCONNECTED, 0x1, 4, BSWAP_NONE);
    uint64_t sig_raw_CELL_UNDERVOLTAGE = (uint64_t)value_CELL_UNDERVOLTAGE;
    payload = CAN_pack_raw_signal(payload, sig_raw_CELL_UNDERVOLTAGE, 0x1, 5, BSWAP_NONE);
    uint64_t sig_raw_CELL_OVERVOLTAGE = (uint64_t)value_CELL_OVERVOLTAGE;
    payload = CAN_pack_raw_signal(payload, sig_raw_CELL_OVERVOLTAGE, 0x1, 6, BSWAP_NONE);
    uint64_t sig_raw_PACK_FULL = (uint64_t)value_PACK_FULL;
    payload = CAN_pack_raw_signal(payload, sig_raw_PACK_FULL, 0x1, 7, BSWAP_NONE);
    uint64_t sig_raw_PACK_EMPTY = (uint64_t)value_PACK_EMPTY;
    payload = CAN_pack_raw_signal(payload, sig_raw_PACK_EMPTY, 0x1, 8, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, A_BOX_FAULT_SYNC_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, CCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, SCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN1
static inline void FDCAN1_init_filters() {
    static const uint32_t sid_list[] = {
        BL_A_BOX_START_MSG_ID,
        DASHBOARD_FAULT_EVENT_MSG_ID,
        MAIN_MODULE_FAULT_EVENT_MSG_ID,
        PDU_FAULT_EVENT_MSG_ID,
        TORQUE_VECTOR_FAULT_EVENT_MSG_ID,
        DASHBOARD_FAULT_SYNC_MSG_ID,
        MAIN_MODULE_FAULT_SYNC_MSG_ID,
        PDU_FAULT_SYNC_MSG_ID,
        TORQUE_VECTOR_FAULT_SYNC_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN1,
        sid_list, 9,
        NULL, 0
    );
}
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {

    static const uint32_t xid_list[] = {
        ELCON_STATUS_MSG_ID,
        CHARGE_REQUEST_MSG_ID
    };

    PHAL_FDCAN_setFilters(
        FDCAN2,
        NULL, 0,
xid_list,
        2
    );
}
// CAN filter setup for FDCAN3
static inline void FDCAN3_init_filters() {
    // No filters configured
    (void)0;
}

#endif // A_BOX_H
