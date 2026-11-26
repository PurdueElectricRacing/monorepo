#ifndef MAIN_MODULE_H
#define MAIN_MODULE_H

/**
 * @file MAIN_MODULE.h
 * @brief Auto-generated CAN driver for MAIN_MODULE.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include <string.h>

#include "can_library/can_codec.h"
#include "can_library/can_common.h"
#include "can_library/generated/can_types.h"
#include "can_library/generated/can_version.h"
#include "can_library/generated/MCAN.h"
#include "can_library/generated/VCAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_MAIN_MODULE uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN1
#define USE_FDCAN2

void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);

// Scaling Macros
// INVA_CRIT
static constexpr float UNPACK_COEFF_INVA_CRIT_AMK_ACTUALTORQUE = 0.1f;
static constexpr float UNPACK_COEFF_INVA_CRIT_AMK_DISPLAYOVERLOADINVERTER = 0.1f;
static constexpr float UNPACK_COEFF_INVA_CRIT_AMK_DISPLAYOVERLOADMOTOR = 0.1f;
// INVA_SET
static constexpr float PACK_COEFF_INVA_SET_AMK_TORQUESETPOINT = 10.0f;
static constexpr float PACK_COEFF_INVA_SET_AMK_POSITIVETORQUELIMIT = 10.0f;
static constexpr float PACK_COEFF_INVA_SET_AMK_NEGATIVETORQUELIMIT = 10.0f;
// INVA_TEMPS
static constexpr float UNPACK_COEFF_INVA_TEMPS_AMK_MOTORTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVA_TEMPS_AMK_INVERTERTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVA_TEMPS_AMK_IGBTTEMP = 0.1f;
// INVB_CRIT
static constexpr float UNPACK_COEFF_INVB_CRIT_AMK_ACTUALTORQUE = 0.1f;
static constexpr float UNPACK_COEFF_INVB_CRIT_AMK_DISPLAYOVERLOADINVERTER = 0.1f;
static constexpr float UNPACK_COEFF_INVB_CRIT_AMK_DISPLAYOVERLOADMOTOR = 0.1f;
// INVB_SET
static constexpr float PACK_COEFF_INVB_SET_AMK_TORQUESETPOINT = 10.0f;
static constexpr float PACK_COEFF_INVB_SET_AMK_POSITIVETORQUELIMIT = 10.0f;
static constexpr float PACK_COEFF_INVB_SET_AMK_NEGATIVETORQUELIMIT = 10.0f;
// INVB_TEMPS
static constexpr float UNPACK_COEFF_INVB_TEMPS_AMK_MOTORTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVB_TEMPS_AMK_INVERTERTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVB_TEMPS_AMK_IGBTTEMP = 0.1f;
// INVC_CRIT
static constexpr float UNPACK_COEFF_INVC_CRIT_AMK_ACTUALTORQUE = 0.1f;
static constexpr float UNPACK_COEFF_INVC_CRIT_AMK_DISPLAYOVERLOADINVERTER = 0.1f;
static constexpr float UNPACK_COEFF_INVC_CRIT_AMK_DISPLAYOVERLOADMOTOR = 0.1f;
// INVC_SET
static constexpr float PACK_COEFF_INVC_SET_AMK_TORQUESETPOINT = 10.0f;
static constexpr float PACK_COEFF_INVC_SET_AMK_POSITIVETORQUELIMIT = 10.0f;
static constexpr float PACK_COEFF_INVC_SET_AMK_NEGATIVETORQUELIMIT = 10.0f;
// INVC_TEMPS
static constexpr float UNPACK_COEFF_INVC_TEMPS_AMK_MOTORTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVC_TEMPS_AMK_INVERTERTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVC_TEMPS_AMK_IGBTTEMP = 0.1f;
// INVD_CRIT
static constexpr float UNPACK_COEFF_INVD_CRIT_AMK_ACTUALTORQUE = 0.1f;
static constexpr float UNPACK_COEFF_INVD_CRIT_AMK_DISPLAYOVERLOADINVERTER = 0.1f;
static constexpr float UNPACK_COEFF_INVD_CRIT_AMK_DISPLAYOVERLOADMOTOR = 0.1f;
// INVD_SET
static constexpr float PACK_COEFF_INVD_SET_AMK_TORQUESETPOINT = 10.0f;
static constexpr float PACK_COEFF_INVD_SET_AMK_POSITIVETORQUELIMIT = 10.0f;
static constexpr float PACK_COEFF_INVD_SET_AMK_NEGATIVETORQUELIMIT = 10.0f;
// INVD_TEMPS
static constexpr float UNPACK_COEFF_INVD_TEMPS_AMK_MOTORTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVD_TEMPS_AMK_INVERTERTEMP = 0.1f;
static constexpr float UNPACK_COEFF_INVD_TEMPS_AMK_IGBTTEMP = 0.1f;
// bms_voltages
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_PACK_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MIN_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MAX_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_AVG_CELL_VOLTAGE = 0.01f;
// igbt_temps
static constexpr float PACK_COEFF_IGBT_TEMPS_FRONT_RIGHT = 10.0f;
static constexpr float PACK_COEFF_IGBT_TEMPS_FRONT_LEFT = 10.0f;
static constexpr float PACK_COEFF_IGBT_TEMPS_REAR_LEFT = 10.0f;
static constexpr float PACK_COEFF_IGBT_TEMPS_REAR_RIGHT = 10.0f;
// motor_temps
static constexpr float PACK_COEFF_MOTOR_TEMPS_FRONT_RIGHT = 10.0f;
static constexpr float PACK_COEFF_MOTOR_TEMPS_FRONT_LEFT = 10.0f;
static constexpr float PACK_COEFF_MOTOR_TEMPS_REAR_LEFT = 10.0f;
static constexpr float PACK_COEFF_MOTOR_TEMPS_REAR_RIGHT = 10.0f;
// vbatt_vmc
static constexpr float PACK_COEFF_VBATT_VMC_VBATT = 10.0f;
static constexpr float PACK_COEFF_VBATT_VMC_VMC = 10.0f;

// Offset Macros

// Data Structures
typedef struct {
    INVA_CRIT_data_t INVA_CRIT;
    INVA_INFO_data_t INVA_INFO;
    INVA_ERR_1_data_t INVA_ERR_1;
    INVA_ERR_2_data_t INVA_ERR_2;
    INVA_TEMPS_data_t INVA_TEMPS;
    INVB_CRIT_data_t INVB_CRIT;
    INVB_INFO_data_t INVB_INFO;
    INVB_ERR_1_data_t INVB_ERR_1;
    INVB_ERR_2_data_t INVB_ERR_2;
    INVB_TEMPS_data_t INVB_TEMPS;
    INVC_CRIT_data_t INVC_CRIT;
    INVC_INFO_data_t INVC_INFO;
    INVC_ERR_1_data_t INVC_ERR_1;
    INVC_ERR_2_data_t INVC_ERR_2;
    INVC_TEMPS_data_t INVC_TEMPS;
    INVD_CRIT_data_t INVD_CRIT;
    INVD_INFO_data_t INVD_INFO;
    INVD_ERR_1_data_t INVD_ERR_1;
    INVD_ERR_2_data_t INVD_ERR_2;
    INVD_TEMPS_data_t INVD_TEMPS;
    pedals_data_t pedals;
    start_button_data_t start_button;
    vcu_settings_data_t vcu_settings;
    vcu_torque_request_data_t vcu_torque_request;
    bms_voltages_data_t bms_voltages;
    bl_main_module_start_data_t bl_main_module_start;
    a_box_fault_event_data_t a_box_fault_event;
    dashboard_fault_event_data_t dashboard_fault_event;
    pdu_fault_event_data_t pdu_fault_event;
    torque_vector_fault_event_data_t torque_vector_fault_event;
    a_box_fault_sync_data_t a_box_fault_sync;
    dashboard_fault_sync_data_t dashboard_fault_sync;
    pdu_fault_sync_data_t pdu_fault_sync;
    torque_vector_fault_sync_data_t torque_vector_fault_sync;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void bl_main_module_start_CALLBACK();
extern void a_box_fault_event_CALLBACK();
extern void dashboard_fault_event_CALLBACK();
extern void pdu_fault_event_CALLBACK();
extern void torque_vector_fault_event_CALLBACK();
extern void a_box_fault_sync_CALLBACK();
extern void dashboard_fault_sync_CALLBACK();
extern void pdu_fault_sync_CALLBACK();
extern void torque_vector_fault_sync_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN1) {
        switch (id) {
            case PEDALS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_throttle = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.pedals.throttle = (uint8_t)sig_raw_throttle;
                uint64_t sig_raw_regen = CAN_unpack_raw_signal(host_data, 0xFF, 8, BSWAP_NONE);
                can_data.pedals.regen = (uint8_t)sig_raw_regen;
                uint64_t sig_raw_brake = CAN_unpack_raw_signal(host_data, 0xFF, 16, BSWAP_NONE);
                can_data.pedals.brake = (uint8_t)sig_raw_brake;

                can_data.pedals.last_rx = xTaskGetTickCount();
                break;
            }
            case START_BUTTON_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_is_pressed = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.start_button.is_pressed = (bool)sig_raw_is_pressed;

                can_data.start_button.last_rx = xTaskGetTickCount();
                break;
            }
            case VCU_SETTINGS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_vcu_mode = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.vcu_settings.vcu_mode = (vcu_mode_t)sig_raw_vcu_mode;
                uint64_t sig_raw_lateral_gain = CAN_unpack_raw_signal(host_data, 0xFF, 8, BSWAP_NONE);
                can_data.vcu_settings.lateral_gain = (uint8_t)sig_raw_lateral_gain;
                uint64_t sig_raw_longitudinal_gain = CAN_unpack_raw_signal(host_data, 0xFF, 16, BSWAP_NONE);
                can_data.vcu_settings.longitudinal_gain = (uint8_t)sig_raw_longitudinal_gain;
                uint64_t sig_raw_electronic_brake_bias = CAN_unpack_raw_signal(host_data, 0xFF, 24, BSWAP_NONE);
                can_data.vcu_settings.electronic_brake_bias = (uint8_t)sig_raw_electronic_brake_bias;
                uint64_t sig_raw_is_regen_enabled = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.vcu_settings.is_regen_enabled = (bool)sig_raw_is_regen_enabled;
                uint64_t sig_raw_is_tv_enabled = CAN_unpack_raw_signal(host_data, 0x1, 33, BSWAP_NONE);
                can_data.vcu_settings.is_tv_enabled = (bool)sig_raw_is_tv_enabled;

                can_data.vcu_settings.last_rx = xTaskGetTickCount();
                break;
            }
            case VCU_TORQUE_REQUEST_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_front_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.vcu_torque_request.front_right = (int16_t)CAN_sign_extend_raw(sig_raw_front_right, 16);
                uint64_t sig_raw_front_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.vcu_torque_request.front_left = (int16_t)CAN_sign_extend_raw(sig_raw_front_left, 16);
                uint64_t sig_raw_rear_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.vcu_torque_request.rear_left = (int16_t)CAN_sign_extend_raw(sig_raw_rear_left, 16);
                uint64_t sig_raw_rear_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.vcu_torque_request.rear_right = (int16_t)CAN_sign_extend_raw(sig_raw_rear_right, 16);

                can_data.vcu_torque_request.last_rx = xTaskGetTickCount();
                break;
            }
            case BMS_VOLTAGES_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_pack_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.bms_voltages.pack_voltage = (uint16_t)sig_raw_pack_voltage;
                uint64_t sig_raw_min_cell_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.bms_voltages.min_cell_voltage = (uint16_t)sig_raw_min_cell_voltage;
                uint64_t sig_raw_max_cell_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.bms_voltages.max_cell_voltage = (uint16_t)sig_raw_max_cell_voltage;
                uint64_t sig_raw_avg_cell_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.bms_voltages.avg_cell_voltage = (uint16_t)sig_raw_avg_cell_voltage;

                can_data.bms_voltages.last_rx = xTaskGetTickCount();
                break;
            }
            case BL_MAIN_MODULE_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_main_module_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_main_module_start.last_rx = xTaskGetTickCount();
                bl_main_module_start_CALLBACK();
                break;
            }
            case A_BOX_FAULT_EVENT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_idx = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.a_box_fault_event.idx = (fault_id_t)sig_raw_idx;
                uint64_t sig_raw_val = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.a_box_fault_event.val = (uint16_t)sig_raw_val;
                uint64_t sig_raw_state = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.a_box_fault_event.state = (bool)sig_raw_state;

                can_data.a_box_fault_event.last_rx = xTaskGetTickCount();
                a_box_fault_event_CALLBACK();
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
            case A_BOX_FAULT_SYNC_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_PACK_OVERTEMP = CAN_unpack_raw_signal(host_data, 0x1, 0, BSWAP_NONE);
                can_data.a_box_fault_sync.PACK_OVERTEMP = (bool)sig_raw_PACK_OVERTEMP;
                uint64_t sig_raw_PACK_WARM = CAN_unpack_raw_signal(host_data, 0x1, 1, BSWAP_NONE);
                can_data.a_box_fault_sync.PACK_WARM = (bool)sig_raw_PACK_WARM;
                uint64_t sig_raw_PACK_COLD = CAN_unpack_raw_signal(host_data, 0x1, 2, BSWAP_NONE);
                can_data.a_box_fault_sync.PACK_COLD = (bool)sig_raw_PACK_COLD;
                uint64_t sig_raw_IMD = CAN_unpack_raw_signal(host_data, 0x1, 3, BSWAP_NONE);
                can_data.a_box_fault_sync.IMD = (bool)sig_raw_IMD;
                uint64_t sig_raw_BMS_DISCONNECTED = CAN_unpack_raw_signal(host_data, 0x1, 4, BSWAP_NONE);
                can_data.a_box_fault_sync.BMS_DISCONNECTED = (bool)sig_raw_BMS_DISCONNECTED;
                uint64_t sig_raw_CELL_UNDERVOLTAGE = CAN_unpack_raw_signal(host_data, 0x1, 5, BSWAP_NONE);
                can_data.a_box_fault_sync.CELL_UNDERVOLTAGE = (bool)sig_raw_CELL_UNDERVOLTAGE;
                uint64_t sig_raw_CELL_OVERVOLTAGE = CAN_unpack_raw_signal(host_data, 0x1, 6, BSWAP_NONE);
                can_data.a_box_fault_sync.CELL_OVERVOLTAGE = (bool)sig_raw_CELL_OVERVOLTAGE;
                uint64_t sig_raw_PACK_FULL = CAN_unpack_raw_signal(host_data, 0x1, 7, BSWAP_NONE);
                can_data.a_box_fault_sync.PACK_FULL = (bool)sig_raw_PACK_FULL;
                uint64_t sig_raw_PACK_EMPTY = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.a_box_fault_sync.PACK_EMPTY = (bool)sig_raw_PACK_EMPTY;

                can_data.a_box_fault_sync.last_rx = xTaskGetTickCount();
                a_box_fault_sync_CALLBACK();
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
            case INVA_CRIT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ActualSpeed = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVA_CRIT.AMK_ActualSpeed = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualSpeed, 16);
                uint64_t sig_raw_AMK_ActualTorque = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVA_CRIT.AMK_ActualTorque = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualTorque, 16);
                uint64_t sig_raw_AMK_DisplayOverloadInverter = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVA_CRIT.AMK_DisplayOverloadInverter = (uint16_t)sig_raw_AMK_DisplayOverloadInverter;
                uint64_t sig_raw_AMK_DisplayOverloadMotor = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.INVA_CRIT.AMK_DisplayOverloadMotor = (uint16_t)sig_raw_AMK_DisplayOverloadMotor;

                can_data.INVA_CRIT.last_rx = xTaskGetTickCount();
                break;
            }
            case INVA_INFO_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_Status_bReserve = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bReserve = (uint8_t)sig_raw_AMK_Status_bReserve;
                uint64_t sig_raw_AMK_Status_bSystemReady = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bSystemReady = (bool)sig_raw_AMK_Status_bSystemReady;
                uint64_t sig_raw_AMK_Status_bError = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bError = (bool)sig_raw_AMK_Status_bError;
                uint64_t sig_raw_AMK_Status_bWarn = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bWarn = (bool)sig_raw_AMK_Status_bWarn;
                uint64_t sig_raw_AMK_Status_bQuitDcOn = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bQuitDcOn = (bool)sig_raw_AMK_Status_bQuitDcOn;
                uint64_t sig_raw_AMK_Status_bDcOn = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bDcOn = (bool)sig_raw_AMK_Status_bDcOn;
                uint64_t sig_raw_AMK_Status_bQuitInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bQuitInverterOn = (bool)sig_raw_AMK_Status_bQuitInverterOn;
                uint64_t sig_raw_AMK_Status_bInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bInverterOn = (bool)sig_raw_AMK_Status_bInverterOn;
                uint64_t sig_raw_AMK_Status_bDerating = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.INVA_INFO.AMK_Status_bDerating = (bool)sig_raw_AMK_Status_bDerating;
                uint64_t sig_raw_AMK_DCBusVoltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVA_INFO.AMK_DCBusVoltage = (uint16_t)sig_raw_AMK_DCBusVoltage;

                can_data.INVA_INFO.last_rx = xTaskGetTickCount();
                break;
            }
            case INVA_ERR_1_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_DiagnosticNumber = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVA_ERR_1.AMK_DiagnosticNumber = (uint32_t)sig_raw_AMK_DiagnosticNumber;
                uint64_t sig_raw_AMK_ErrorInfo1 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVA_ERR_1.AMK_ErrorInfo1 = (uint32_t)sig_raw_AMK_ErrorInfo1;

                can_data.INVA_ERR_1.last_rx = xTaskGetTickCount();
                break;
            }
            case INVA_ERR_2_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ErrorInfo2 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVA_ERR_2.AMK_ErrorInfo2 = (uint32_t)sig_raw_AMK_ErrorInfo2;
                uint64_t sig_raw_AMK_ErrorInfo3 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVA_ERR_2.AMK_ErrorInfo3 = (uint32_t)sig_raw_AMK_ErrorInfo3;

                can_data.INVA_ERR_2.last_rx = xTaskGetTickCount();
                break;
            }
            case INVA_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_MotorTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVA_TEMPS.AMK_MotorTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_MotorTemp, 16);
                uint64_t sig_raw_AMK_InverterTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVA_TEMPS.AMK_InverterTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_InverterTemp, 16);
                uint64_t sig_raw_AMK_IGBTTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVA_TEMPS.AMK_IGBTTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_IGBTTemp, 16);

                can_data.INVA_TEMPS.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_CRIT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ActualSpeed = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVB_CRIT.AMK_ActualSpeed = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualSpeed, 16);
                uint64_t sig_raw_AMK_ActualTorque = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVB_CRIT.AMK_ActualTorque = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualTorque, 16);
                uint64_t sig_raw_AMK_DisplayOverloadInverter = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVB_CRIT.AMK_DisplayOverloadInverter = (uint16_t)sig_raw_AMK_DisplayOverloadInverter;
                uint64_t sig_raw_AMK_DisplayOverloadMotor = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.INVB_CRIT.AMK_DisplayOverloadMotor = (uint16_t)sig_raw_AMK_DisplayOverloadMotor;

                can_data.INVB_CRIT.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_INFO_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_Status_bReserve = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bReserve = (uint8_t)sig_raw_AMK_Status_bReserve;
                uint64_t sig_raw_AMK_Status_bSystemReady = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bSystemReady = (bool)sig_raw_AMK_Status_bSystemReady;
                uint64_t sig_raw_AMK_Status_bError = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bError = (bool)sig_raw_AMK_Status_bError;
                uint64_t sig_raw_AMK_Status_bWarn = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bWarn = (bool)sig_raw_AMK_Status_bWarn;
                uint64_t sig_raw_AMK_Status_bQuitDcOn = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bQuitDcOn = (bool)sig_raw_AMK_Status_bQuitDcOn;
                uint64_t sig_raw_AMK_Status_bDcOn = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bDcOn = (bool)sig_raw_AMK_Status_bDcOn;
                uint64_t sig_raw_AMK_Status_bQuitInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bQuitInverterOn = (bool)sig_raw_AMK_Status_bQuitInverterOn;
                uint64_t sig_raw_AMK_Status_bInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bInverterOn = (bool)sig_raw_AMK_Status_bInverterOn;
                uint64_t sig_raw_AMK_Status_bDerating = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.INVB_INFO.AMK_Status_bDerating = (bool)sig_raw_AMK_Status_bDerating;
                uint64_t sig_raw_AMK_DCBusVoltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVB_INFO.AMK_DCBusVoltage = (uint16_t)sig_raw_AMK_DCBusVoltage;

                can_data.INVB_INFO.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_ERR_1_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_DiagnosticNumber = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVB_ERR_1.AMK_DiagnosticNumber = (uint32_t)sig_raw_AMK_DiagnosticNumber;
                uint64_t sig_raw_AMK_ErrorInfo1 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVB_ERR_1.AMK_ErrorInfo1 = (uint32_t)sig_raw_AMK_ErrorInfo1;

                can_data.INVB_ERR_1.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_ERR_2_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ErrorInfo2 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVB_ERR_2.AMK_ErrorInfo2 = (uint32_t)sig_raw_AMK_ErrorInfo2;
                uint64_t sig_raw_AMK_ErrorInfo3 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVB_ERR_2.AMK_ErrorInfo3 = (uint32_t)sig_raw_AMK_ErrorInfo3;

                can_data.INVB_ERR_2.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_MotorTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVB_TEMPS.AMK_MotorTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_MotorTemp, 16);
                uint64_t sig_raw_AMK_InverterTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVB_TEMPS.AMK_InverterTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_InverterTemp, 16);
                uint64_t sig_raw_AMK_IGBTTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVB_TEMPS.AMK_IGBTTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_IGBTTemp, 16);

                can_data.INVB_TEMPS.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_CRIT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ActualSpeed = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVC_CRIT.AMK_ActualSpeed = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualSpeed, 16);
                uint64_t sig_raw_AMK_ActualTorque = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVC_CRIT.AMK_ActualTorque = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualTorque, 16);
                uint64_t sig_raw_AMK_DisplayOverloadInverter = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVC_CRIT.AMK_DisplayOverloadInverter = (uint16_t)sig_raw_AMK_DisplayOverloadInverter;
                uint64_t sig_raw_AMK_DisplayOverloadMotor = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.INVC_CRIT.AMK_DisplayOverloadMotor = (uint16_t)sig_raw_AMK_DisplayOverloadMotor;

                can_data.INVC_CRIT.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_INFO_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_Status_bReserve = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bReserve = (uint8_t)sig_raw_AMK_Status_bReserve;
                uint64_t sig_raw_AMK_Status_bSystemReady = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bSystemReady = (bool)sig_raw_AMK_Status_bSystemReady;
                uint64_t sig_raw_AMK_Status_bError = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bError = (bool)sig_raw_AMK_Status_bError;
                uint64_t sig_raw_AMK_Status_bWarn = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bWarn = (bool)sig_raw_AMK_Status_bWarn;
                uint64_t sig_raw_AMK_Status_bQuitDcOn = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bQuitDcOn = (bool)sig_raw_AMK_Status_bQuitDcOn;
                uint64_t sig_raw_AMK_Status_bDcOn = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bDcOn = (bool)sig_raw_AMK_Status_bDcOn;
                uint64_t sig_raw_AMK_Status_bQuitInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bQuitInverterOn = (bool)sig_raw_AMK_Status_bQuitInverterOn;
                uint64_t sig_raw_AMK_Status_bInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bInverterOn = (bool)sig_raw_AMK_Status_bInverterOn;
                uint64_t sig_raw_AMK_Status_bDerating = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.INVC_INFO.AMK_Status_bDerating = (bool)sig_raw_AMK_Status_bDerating;
                uint64_t sig_raw_AMK_DCBusVoltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVC_INFO.AMK_DCBusVoltage = (uint16_t)sig_raw_AMK_DCBusVoltage;

                can_data.INVC_INFO.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_ERR_1_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_DiagnosticNumber = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVC_ERR_1.AMK_DiagnosticNumber = (uint32_t)sig_raw_AMK_DiagnosticNumber;
                uint64_t sig_raw_AMK_ErrorInfo1 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVC_ERR_1.AMK_ErrorInfo1 = (uint32_t)sig_raw_AMK_ErrorInfo1;

                can_data.INVC_ERR_1.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_ERR_2_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ErrorInfo2 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVC_ERR_2.AMK_ErrorInfo2 = (uint32_t)sig_raw_AMK_ErrorInfo2;
                uint64_t sig_raw_AMK_ErrorInfo3 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVC_ERR_2.AMK_ErrorInfo3 = (uint32_t)sig_raw_AMK_ErrorInfo3;

                can_data.INVC_ERR_2.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_MotorTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVC_TEMPS.AMK_MotorTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_MotorTemp, 16);
                uint64_t sig_raw_AMK_InverterTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVC_TEMPS.AMK_InverterTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_InverterTemp, 16);
                uint64_t sig_raw_AMK_IGBTTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVC_TEMPS.AMK_IGBTTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_IGBTTemp, 16);

                can_data.INVC_TEMPS.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_CRIT_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ActualSpeed = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVD_CRIT.AMK_ActualSpeed = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualSpeed, 16);
                uint64_t sig_raw_AMK_ActualTorque = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVD_CRIT.AMK_ActualTorque = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_ActualTorque, 16);
                uint64_t sig_raw_AMK_DisplayOverloadInverter = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVD_CRIT.AMK_DisplayOverloadInverter = (uint16_t)sig_raw_AMK_DisplayOverloadInverter;
                uint64_t sig_raw_AMK_DisplayOverloadMotor = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.INVD_CRIT.AMK_DisplayOverloadMotor = (uint16_t)sig_raw_AMK_DisplayOverloadMotor;

                can_data.INVD_CRIT.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_INFO_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_Status_bReserve = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bReserve = (uint8_t)sig_raw_AMK_Status_bReserve;
                uint64_t sig_raw_AMK_Status_bSystemReady = CAN_unpack_raw_signal(host_data, 0x1, 8, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bSystemReady = (bool)sig_raw_AMK_Status_bSystemReady;
                uint64_t sig_raw_AMK_Status_bError = CAN_unpack_raw_signal(host_data, 0x1, 9, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bError = (bool)sig_raw_AMK_Status_bError;
                uint64_t sig_raw_AMK_Status_bWarn = CAN_unpack_raw_signal(host_data, 0x1, 10, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bWarn = (bool)sig_raw_AMK_Status_bWarn;
                uint64_t sig_raw_AMK_Status_bQuitDcOn = CAN_unpack_raw_signal(host_data, 0x1, 11, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bQuitDcOn = (bool)sig_raw_AMK_Status_bQuitDcOn;
                uint64_t sig_raw_AMK_Status_bDcOn = CAN_unpack_raw_signal(host_data, 0x1, 12, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bDcOn = (bool)sig_raw_AMK_Status_bDcOn;
                uint64_t sig_raw_AMK_Status_bQuitInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 13, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bQuitInverterOn = (bool)sig_raw_AMK_Status_bQuitInverterOn;
                uint64_t sig_raw_AMK_Status_bInverterOn = CAN_unpack_raw_signal(host_data, 0x1, 14, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bInverterOn = (bool)sig_raw_AMK_Status_bInverterOn;
                uint64_t sig_raw_AMK_Status_bDerating = CAN_unpack_raw_signal(host_data, 0x1, 15, BSWAP_NONE);
                can_data.INVD_INFO.AMK_Status_bDerating = (bool)sig_raw_AMK_Status_bDerating;
                uint64_t sig_raw_AMK_DCBusVoltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVD_INFO.AMK_DCBusVoltage = (uint16_t)sig_raw_AMK_DCBusVoltage;

                can_data.INVD_INFO.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_ERR_1_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_DiagnosticNumber = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVD_ERR_1.AMK_DiagnosticNumber = (uint32_t)sig_raw_AMK_DiagnosticNumber;
                uint64_t sig_raw_AMK_ErrorInfo1 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVD_ERR_1.AMK_ErrorInfo1 = (uint32_t)sig_raw_AMK_ErrorInfo1;

                can_data.INVD_ERR_1.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_ERR_2_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_ErrorInfo2 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.INVD_ERR_2.AMK_ErrorInfo2 = (uint32_t)sig_raw_AMK_ErrorInfo2;
                uint64_t sig_raw_AMK_ErrorInfo3 = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.INVD_ERR_2.AMK_ErrorInfo3 = (uint32_t)sig_raw_AMK_ErrorInfo3;

                can_data.INVD_ERR_2.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_AMK_MotorTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.INVD_TEMPS.AMK_MotorTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_MotorTemp, 16);
                uint64_t sig_raw_AMK_InverterTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.INVD_TEMPS.AMK_InverterTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_InverterTemp, 16);
                uint64_t sig_raw_AMK_IGBTTemp = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.INVD_TEMPS.AMK_IGBTTemp = (int16_t)CAN_sign_extend_raw(sig_raw_AMK_IGBTTemp, 16);

                can_data.INVD_TEMPS.last_rx = xTaskGetTickCount();
                break;
            }
            default:
                break;
        }
    }
}

// Stale Checking
static bool is_INVA_CRIT_stale() {
    return (xTaskGetTickCount() - can_data.INVA_CRIT.last_rx) > INVA_CRIT_STALE_TIMEOUT_MS;
}
static bool is_INVA_INFO_stale() {
    return (xTaskGetTickCount() - can_data.INVA_INFO.last_rx) > INVA_INFO_STALE_TIMEOUT_MS;
}
static bool is_INVA_ERR_1_stale() {
    return (xTaskGetTickCount() - can_data.INVA_ERR_1.last_rx) > INVA_ERR_1_STALE_TIMEOUT_MS;
}
static bool is_INVA_ERR_2_stale() {
    return (xTaskGetTickCount() - can_data.INVA_ERR_2.last_rx) > INVA_ERR_2_STALE_TIMEOUT_MS;
}
static bool is_INVA_TEMPS_stale() {
    return (xTaskGetTickCount() - can_data.INVA_TEMPS.last_rx) > INVA_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_INVB_CRIT_stale() {
    return (xTaskGetTickCount() - can_data.INVB_CRIT.last_rx) > INVB_CRIT_STALE_TIMEOUT_MS;
}
static bool is_INVB_INFO_stale() {
    return (xTaskGetTickCount() - can_data.INVB_INFO.last_rx) > INVB_INFO_STALE_TIMEOUT_MS;
}
static bool is_INVB_ERR_1_stale() {
    return (xTaskGetTickCount() - can_data.INVB_ERR_1.last_rx) > INVB_ERR_1_STALE_TIMEOUT_MS;
}
static bool is_INVB_ERR_2_stale() {
    return (xTaskGetTickCount() - can_data.INVB_ERR_2.last_rx) > INVB_ERR_2_STALE_TIMEOUT_MS;
}
static bool is_INVB_TEMPS_stale() {
    return (xTaskGetTickCount() - can_data.INVB_TEMPS.last_rx) > INVB_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_INVC_CRIT_stale() {
    return (xTaskGetTickCount() - can_data.INVC_CRIT.last_rx) > INVC_CRIT_STALE_TIMEOUT_MS;
}
static bool is_INVC_INFO_stale() {
    return (xTaskGetTickCount() - can_data.INVC_INFO.last_rx) > INVC_INFO_STALE_TIMEOUT_MS;
}
static bool is_INVC_ERR_1_stale() {
    return (xTaskGetTickCount() - can_data.INVC_ERR_1.last_rx) > INVC_ERR_1_STALE_TIMEOUT_MS;
}
static bool is_INVC_ERR_2_stale() {
    return (xTaskGetTickCount() - can_data.INVC_ERR_2.last_rx) > INVC_ERR_2_STALE_TIMEOUT_MS;
}
static bool is_INVC_TEMPS_stale() {
    return (xTaskGetTickCount() - can_data.INVC_TEMPS.last_rx) > INVC_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_INVD_CRIT_stale() {
    return (xTaskGetTickCount() - can_data.INVD_CRIT.last_rx) > INVD_CRIT_STALE_TIMEOUT_MS;
}
static bool is_INVD_INFO_stale() {
    return (xTaskGetTickCount() - can_data.INVD_INFO.last_rx) > INVD_INFO_STALE_TIMEOUT_MS;
}
static bool is_INVD_ERR_1_stale() {
    return (xTaskGetTickCount() - can_data.INVD_ERR_1.last_rx) > INVD_ERR_1_STALE_TIMEOUT_MS;
}
static bool is_INVD_ERR_2_stale() {
    return (xTaskGetTickCount() - can_data.INVD_ERR_2.last_rx) > INVD_ERR_2_STALE_TIMEOUT_MS;
}
static bool is_INVD_TEMPS_stale() {
    return (xTaskGetTickCount() - can_data.INVD_TEMPS.last_rx) > INVD_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_pedals_stale() {
    return (xTaskGetTickCount() - can_data.pedals.last_rx) > PEDALS_STALE_TIMEOUT_MS;
}
static bool is_start_button_stale() {
    return (xTaskGetTickCount() - can_data.start_button.last_rx) > START_BUTTON_STALE_TIMEOUT_MS;
}
static bool is_vcu_settings_stale() {
    return (xTaskGetTickCount() - can_data.vcu_settings.last_rx) > VCU_SETTINGS_STALE_TIMEOUT_MS;
}
static bool is_vcu_torque_request_stale() {
    return (xTaskGetTickCount() - can_data.vcu_torque_request.last_rx) > VCU_TORQUE_REQUEST_STALE_TIMEOUT_MS;
}
static bool is_bms_voltages_stale() {
    return (xTaskGetTickCount() - can_data.bms_voltages.last_rx) > BMS_VOLTAGES_STALE_TIMEOUT_MS;
}
static bool is_a_box_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.a_box_fault_sync.last_rx) > A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS;
}
static bool is_dashboard_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.dashboard_fault_sync.last_rx) > DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS;
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
    can_data.INVA_CRIT.last_rx = (uint32_t) -(INVA_CRIT_STALE_TIMEOUT_MS + 1);
    can_data.INVA_CRIT.is_stale = is_INVA_CRIT_stale;
    can_data.INVA_INFO.last_rx = (uint32_t) -(INVA_INFO_STALE_TIMEOUT_MS + 1);
    can_data.INVA_INFO.is_stale = is_INVA_INFO_stale;
    can_data.INVA_ERR_1.last_rx = (uint32_t) -(INVA_ERR_1_STALE_TIMEOUT_MS + 1);
    can_data.INVA_ERR_1.is_stale = is_INVA_ERR_1_stale;
    can_data.INVA_ERR_2.last_rx = (uint32_t) -(INVA_ERR_2_STALE_TIMEOUT_MS + 1);
    can_data.INVA_ERR_2.is_stale = is_INVA_ERR_2_stale;
    can_data.INVA_TEMPS.last_rx = (uint32_t) -(INVA_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.INVA_TEMPS.is_stale = is_INVA_TEMPS_stale;
    can_data.INVB_CRIT.last_rx = (uint32_t) -(INVB_CRIT_STALE_TIMEOUT_MS + 1);
    can_data.INVB_CRIT.is_stale = is_INVB_CRIT_stale;
    can_data.INVB_INFO.last_rx = (uint32_t) -(INVB_INFO_STALE_TIMEOUT_MS + 1);
    can_data.INVB_INFO.is_stale = is_INVB_INFO_stale;
    can_data.INVB_ERR_1.last_rx = (uint32_t) -(INVB_ERR_1_STALE_TIMEOUT_MS + 1);
    can_data.INVB_ERR_1.is_stale = is_INVB_ERR_1_stale;
    can_data.INVB_ERR_2.last_rx = (uint32_t) -(INVB_ERR_2_STALE_TIMEOUT_MS + 1);
    can_data.INVB_ERR_2.is_stale = is_INVB_ERR_2_stale;
    can_data.INVB_TEMPS.last_rx = (uint32_t) -(INVB_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.INVB_TEMPS.is_stale = is_INVB_TEMPS_stale;
    can_data.INVC_CRIT.last_rx = (uint32_t) -(INVC_CRIT_STALE_TIMEOUT_MS + 1);
    can_data.INVC_CRIT.is_stale = is_INVC_CRIT_stale;
    can_data.INVC_INFO.last_rx = (uint32_t) -(INVC_INFO_STALE_TIMEOUT_MS + 1);
    can_data.INVC_INFO.is_stale = is_INVC_INFO_stale;
    can_data.INVC_ERR_1.last_rx = (uint32_t) -(INVC_ERR_1_STALE_TIMEOUT_MS + 1);
    can_data.INVC_ERR_1.is_stale = is_INVC_ERR_1_stale;
    can_data.INVC_ERR_2.last_rx = (uint32_t) -(INVC_ERR_2_STALE_TIMEOUT_MS + 1);
    can_data.INVC_ERR_2.is_stale = is_INVC_ERR_2_stale;
    can_data.INVC_TEMPS.last_rx = (uint32_t) -(INVC_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.INVC_TEMPS.is_stale = is_INVC_TEMPS_stale;
    can_data.INVD_CRIT.last_rx = (uint32_t) -(INVD_CRIT_STALE_TIMEOUT_MS + 1);
    can_data.INVD_CRIT.is_stale = is_INVD_CRIT_stale;
    can_data.INVD_INFO.last_rx = (uint32_t) -(INVD_INFO_STALE_TIMEOUT_MS + 1);
    can_data.INVD_INFO.is_stale = is_INVD_INFO_stale;
    can_data.INVD_ERR_1.last_rx = (uint32_t) -(INVD_ERR_1_STALE_TIMEOUT_MS + 1);
    can_data.INVD_ERR_1.is_stale = is_INVD_ERR_1_stale;
    can_data.INVD_ERR_2.last_rx = (uint32_t) -(INVD_ERR_2_STALE_TIMEOUT_MS + 1);
    can_data.INVD_ERR_2.is_stale = is_INVD_ERR_2_stale;
    can_data.INVD_TEMPS.last_rx = (uint32_t) -(INVD_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.INVD_TEMPS.is_stale = is_INVD_TEMPS_stale;
    can_data.pedals.last_rx = (uint32_t) -(PEDALS_STALE_TIMEOUT_MS + 1);
    can_data.pedals.is_stale = is_pedals_stale;
    can_data.start_button.last_rx = (uint32_t) -(START_BUTTON_STALE_TIMEOUT_MS + 1);
    can_data.start_button.is_stale = is_start_button_stale;
    can_data.vcu_settings.last_rx = (uint32_t) -(VCU_SETTINGS_STALE_TIMEOUT_MS + 1);
    can_data.vcu_settings.is_stale = is_vcu_settings_stale;
    can_data.vcu_torque_request.last_rx = (uint32_t) -(VCU_TORQUE_REQUEST_STALE_TIMEOUT_MS + 1);
    can_data.vcu_torque_request.is_stale = is_vcu_torque_request_stale;
    can_data.bms_voltages.last_rx = (uint32_t) -(BMS_VOLTAGES_STALE_TIMEOUT_MS + 1);
    can_data.bms_voltages.is_stale = is_bms_voltages_stale;
    can_data.a_box_fault_sync.last_rx = (uint32_t) -(A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.a_box_fault_sync.is_stale = is_a_box_fault_sync_stale;
    can_data.dashboard_fault_sync.last_rx = (uint32_t) -(DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.dashboard_fault_sync.is_stale = is_dashboard_fault_sync_stale;
    can_data.pdu_fault_sync.last_rx = (uint32_t) -(PDU_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.pdu_fault_sync.is_stale = is_pdu_fault_sync_stale;
    can_data.torque_vector_fault_sync.last_rx = (uint32_t) -(TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.torque_vector_fault_sync.is_stale = is_torque_vector_fault_sync_stale;
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_main_hb_amk(
    uint8_t value_car_state,
    bool value_precharge_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = MAIN_HB_AMK_MSG_ID,
        .IDE = false,
        .DLC = MAIN_HB_AMK_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_car_state = (uint64_t)value_car_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_car_state, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_precharge_state = (uint64_t)value_precharge_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_precharge_state, 0x1, 8, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_HB_AMK_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_INVA_SET(
    uint8_t value_AMK_Control_bReserve,
    bool value_AMK_Control_bInverterOn,
    bool value_AMK_Control_bDcOn,
    bool value_AMK_Control_bEnable,
    bool value_AMK_Control_bErrorReset,
    uint8_t value_AMK_Control_bReserve2,
    int16_t value_AMK_TorqueSetpoint,
    int16_t value_AMK_PositiveTorqueLimit,
    int16_t value_AMK_NegativeTorqueLimit
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = INVA_SET_MSG_ID,
        .IDE = false,
        .DLC = INVA_SET_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_AMK_Control_bReserve = (uint64_t)value_AMK_Control_bReserve;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bInverterOn = (uint64_t)value_AMK_Control_bInverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bInverterOn, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bDcOn = (uint64_t)value_AMK_Control_bDcOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bDcOn, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bEnable = (uint64_t)value_AMK_Control_bEnable;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bEnable, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bErrorReset = (uint64_t)value_AMK_Control_bErrorReset;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bErrorReset, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bReserve2 = (uint64_t)value_AMK_Control_bReserve2;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve2, 0xF, 12, BSWAP_NONE);
    uint64_t sig_raw_AMK_TorqueSetpoint = (uint64_t)value_AMK_TorqueSetpoint;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_TorqueSetpoint, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_AMK_PositiveTorqueLimit = (uint64_t)value_AMK_PositiveTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_PositiveTorqueLimit, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_AMK_NegativeTorqueLimit = (uint64_t)value_AMK_NegativeTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_NegativeTorqueLimit, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVA_SET_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_INVB_SET(
    uint8_t value_AMK_Control_bReserve,
    bool value_AMK_Control_bInverterOn,
    bool value_AMK_Control_bDcOn,
    bool value_AMK_Control_bEnable,
    bool value_AMK_Control_bErrorReset,
    uint8_t value_AMK_Control_bReserve2,
    int16_t value_AMK_TorqueSetpoint,
    int16_t value_AMK_PositiveTorqueLimit,
    int16_t value_AMK_NegativeTorqueLimit
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = INVB_SET_MSG_ID,
        .IDE = false,
        .DLC = INVB_SET_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_AMK_Control_bReserve = (uint64_t)value_AMK_Control_bReserve;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bInverterOn = (uint64_t)value_AMK_Control_bInverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bInverterOn, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bDcOn = (uint64_t)value_AMK_Control_bDcOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bDcOn, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bEnable = (uint64_t)value_AMK_Control_bEnable;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bEnable, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bErrorReset = (uint64_t)value_AMK_Control_bErrorReset;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bErrorReset, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bReserve2 = (uint64_t)value_AMK_Control_bReserve2;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve2, 0xF, 12, BSWAP_NONE);
    uint64_t sig_raw_AMK_TorqueSetpoint = (uint64_t)value_AMK_TorqueSetpoint;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_TorqueSetpoint, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_AMK_PositiveTorqueLimit = (uint64_t)value_AMK_PositiveTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_PositiveTorqueLimit, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_AMK_NegativeTorqueLimit = (uint64_t)value_AMK_NegativeTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_NegativeTorqueLimit, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVB_SET_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_INVC_SET(
    uint8_t value_AMK_Control_bReserve,
    bool value_AMK_Control_bInverterOn,
    bool value_AMK_Control_bDcOn,
    bool value_AMK_Control_bEnable,
    bool value_AMK_Control_bErrorReset,
    uint8_t value_AMK_Control_bReserve2,
    int16_t value_AMK_TorqueSetpoint,
    int16_t value_AMK_PositiveTorqueLimit,
    int16_t value_AMK_NegativeTorqueLimit
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = INVC_SET_MSG_ID,
        .IDE = false,
        .DLC = INVC_SET_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_AMK_Control_bReserve = (uint64_t)value_AMK_Control_bReserve;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bInverterOn = (uint64_t)value_AMK_Control_bInverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bInverterOn, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bDcOn = (uint64_t)value_AMK_Control_bDcOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bDcOn, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bEnable = (uint64_t)value_AMK_Control_bEnable;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bEnable, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bErrorReset = (uint64_t)value_AMK_Control_bErrorReset;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bErrorReset, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bReserve2 = (uint64_t)value_AMK_Control_bReserve2;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve2, 0xF, 12, BSWAP_NONE);
    uint64_t sig_raw_AMK_TorqueSetpoint = (uint64_t)value_AMK_TorqueSetpoint;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_TorqueSetpoint, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_AMK_PositiveTorqueLimit = (uint64_t)value_AMK_PositiveTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_PositiveTorqueLimit, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_AMK_NegativeTorqueLimit = (uint64_t)value_AMK_NegativeTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_NegativeTorqueLimit, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVC_SET_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_INVD_SET(
    uint8_t value_AMK_Control_bReserve,
    bool value_AMK_Control_bInverterOn,
    bool value_AMK_Control_bDcOn,
    bool value_AMK_Control_bEnable,
    bool value_AMK_Control_bErrorReset,
    uint8_t value_AMK_Control_bReserve2,
    int16_t value_AMK_TorqueSetpoint,
    int16_t value_AMK_PositiveTorqueLimit,
    int16_t value_AMK_NegativeTorqueLimit
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = INVD_SET_MSG_ID,
        .IDE = false,
        .DLC = INVD_SET_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_AMK_Control_bReserve = (uint64_t)value_AMK_Control_bReserve;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bInverterOn = (uint64_t)value_AMK_Control_bInverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bInverterOn, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bDcOn = (uint64_t)value_AMK_Control_bDcOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bDcOn, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bEnable = (uint64_t)value_AMK_Control_bEnable;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bEnable, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bErrorReset = (uint64_t)value_AMK_Control_bErrorReset;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bErrorReset, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_AMK_Control_bReserve2 = (uint64_t)value_AMK_Control_bReserve2;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_Control_bReserve2, 0xF, 12, BSWAP_NONE);
    uint64_t sig_raw_AMK_TorqueSetpoint = (uint64_t)value_AMK_TorqueSetpoint;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_TorqueSetpoint, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_AMK_PositiveTorqueLimit = (uint64_t)value_AMK_PositiveTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_PositiveTorqueLimit, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_AMK_NegativeTorqueLimit = (uint64_t)value_AMK_NegativeTorqueLimit;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK_NegativeTorqueLimit, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVD_SET_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_INIT_MSG_ID,
        .IDE = false,
        .DLC = MAIN_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_INIT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_hb(
    car_state_t value_car_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_HB_MSG_ID,
        .IDE = false,
        .DLC = MAIN_HB_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_car_state = (uint64_t)value_car_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_car_state, 0xFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_HB_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_motor_temps(
    int16_t value_front_right,
    int16_t value_front_left,
    int16_t value_rear_left,
    int16_t value_rear_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MOTOR_TEMPS_MSG_ID,
        .IDE = false,
        .DLC = MOTOR_TEMPS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_front_right = (uint64_t)value_front_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_right, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_front_left = (uint64_t)value_front_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_left, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_rear_left = (uint64_t)value_rear_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_left, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_rear_right = (uint64_t)value_rear_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_right, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MOTOR_TEMPS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_igbt_temps(
    int16_t value_front_right,
    int16_t value_front_left,
    int16_t value_rear_left,
    int16_t value_rear_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = IGBT_TEMPS_MSG_ID,
        .IDE = false,
        .DLC = IGBT_TEMPS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_front_right = (uint64_t)value_front_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_right, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_front_left = (uint64_t)value_front_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_left, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_rear_left = (uint64_t)value_rear_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_left, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_rear_right = (uint64_t)value_rear_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_right, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IGBT_TEMPS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_motor_speeds(
    int16_t value_front_right,
    int16_t value_front_left,
    int16_t value_rear_left,
    int16_t value_rear_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MOTOR_SPEEDS_MSG_ID,
        .IDE = false,
        .DLC = MOTOR_SPEEDS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_front_right = (uint64_t)value_front_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_right, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_front_left = (uint64_t)value_front_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_front_left, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_rear_left = (uint64_t)value_rear_left;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_left, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_rear_right = (uint64_t)value_rear_right;
    payload = CAN_pack_raw_signal(payload, sig_raw_rear_right, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MOTOR_SPEEDS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_vbatt_vmc(
    uint16_t value_vbatt,
    uint16_t value_vmc
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = VBATT_VMC_MSG_ID,
        .IDE = false,
        .DLC = VBATT_VMC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_vbatt = (uint64_t)value_vbatt;
    payload = CAN_pack_raw_signal(payload, sig_raw_vbatt, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_vmc = (uint64_t)value_vmc;
    payload = CAN_pack_raw_signal(payload, sig_raw_vmc, 0xFFFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, VBATT_VMC_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_VERSION_MSG_ID,
        .IDE = false,
        .DLC = MAIN_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_VERSION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_inva_diagnostics(
    uint32_t value_diagnostic_number,
    AMK_state_t value_amk_state,
    bool value_bError,
    bool value_InverterOn,
    bool value_is_any_stale
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = INVA_DIAGNOSTICS_MSG_ID,
        .IDE = false,
        .DLC = INVA_DIAGNOSTICS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_diagnostic_number = (uint64_t)value_diagnostic_number;
    payload = CAN_pack_raw_signal(payload, sig_raw_diagnostic_number, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_amk_state = (uint64_t)value_amk_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_amk_state, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_bError = (uint64_t)value_bError;
    payload = CAN_pack_raw_signal(payload, sig_raw_bError, 0x1, 40, BSWAP_NONE);
    uint64_t sig_raw_InverterOn = (uint64_t)value_InverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_InverterOn, 0x1, 41, BSWAP_NONE);
    uint64_t sig_raw_is_any_stale = (uint64_t)value_is_any_stale;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_any_stale, 0x1, 42, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVA_DIAGNOSTICS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_invb_diagnostics(
    uint32_t value_diagnostic_number,
    AMK_state_t value_amk_state,
    bool value_bError,
    bool value_InverterOn,
    bool value_is_any_stale
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = INVB_DIAGNOSTICS_MSG_ID,
        .IDE = false,
        .DLC = INVB_DIAGNOSTICS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_diagnostic_number = (uint64_t)value_diagnostic_number;
    payload = CAN_pack_raw_signal(payload, sig_raw_diagnostic_number, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_amk_state = (uint64_t)value_amk_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_amk_state, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_bError = (uint64_t)value_bError;
    payload = CAN_pack_raw_signal(payload, sig_raw_bError, 0x1, 40, BSWAP_NONE);
    uint64_t sig_raw_InverterOn = (uint64_t)value_InverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_InverterOn, 0x1, 41, BSWAP_NONE);
    uint64_t sig_raw_is_any_stale = (uint64_t)value_is_any_stale;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_any_stale, 0x1, 42, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVB_DIAGNOSTICS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_invc_diagnostics(
    uint32_t value_diagnostic_number,
    AMK_state_t value_amk_state,
    bool value_bError,
    bool value_InverterOn,
    bool value_is_any_stale
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = INVC_DIAGNOSTICS_MSG_ID,
        .IDE = false,
        .DLC = INVC_DIAGNOSTICS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_diagnostic_number = (uint64_t)value_diagnostic_number;
    payload = CAN_pack_raw_signal(payload, sig_raw_diagnostic_number, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_amk_state = (uint64_t)value_amk_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_amk_state, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_bError = (uint64_t)value_bError;
    payload = CAN_pack_raw_signal(payload, sig_raw_bError, 0x1, 40, BSWAP_NONE);
    uint64_t sig_raw_InverterOn = (uint64_t)value_InverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_InverterOn, 0x1, 41, BSWAP_NONE);
    uint64_t sig_raw_is_any_stale = (uint64_t)value_is_any_stale;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_any_stale, 0x1, 42, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVC_DIAGNOSTICS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_invd_diagnostics(
    uint32_t value_diagnostic_number,
    AMK_state_t value_amk_state,
    bool value_bError,
    bool value_InverterOn,
    bool value_is_any_stale
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = INVD_DIAGNOSTICS_MSG_ID,
        .IDE = false,
        .DLC = INVD_DIAGNOSTICS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_diagnostic_number = (uint64_t)value_diagnostic_number;
    payload = CAN_pack_raw_signal(payload, sig_raw_diagnostic_number, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_amk_state = (uint64_t)value_amk_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_amk_state, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_bError = (uint64_t)value_bError;
    payload = CAN_pack_raw_signal(payload, sig_raw_bError, 0x1, 40, BSWAP_NONE);
    uint64_t sig_raw_InverterOn = (uint64_t)value_InverterOn;
    payload = CAN_pack_raw_signal(payload, sig_raw_InverterOn, 0x1, 41, BSWAP_NONE);
    uint64_t sig_raw_is_any_stale = (uint64_t)value_is_any_stale;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_any_stale, 0x1, 42, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, INVD_DIAGNOSTICS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = MAIN_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = MAIN_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = MAIN_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_module_fault_event(
    fault_id_t value_idx,
    uint16_t value_val,
    bool value_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_MODULE_FAULT_EVENT_MSG_ID,
        .IDE = false,
        .DLC = MAIN_MODULE_FAULT_EVENT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_idx = (uint64_t)value_idx;
    payload = CAN_pack_raw_signal(payload, sig_raw_idx, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_val = (uint64_t)value_val;
    payload = CAN_pack_raw_signal(payload, sig_raw_val, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_state = (uint64_t)value_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_state, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_MODULE_FAULT_EVENT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_main_module_fault_sync(
    bool value_SDC1_IMD,
    bool value_SDC2_BMS,
    bool value_SDC3_BSPD,
    bool value_SDC4_ECU_OK,
    bool value_SDC5_BOTS,
    bool value_SDC6_INERTIA,
    bool value_SDC7_COCKPIT_ESTOP,
    bool value_SDC9_FRONT_INTERLOCK,
    bool value_SDC10_RIGHT_ESTOP,
    bool value_SDC11_LEFT_ESTOP,
    bool value_SDC12_MSD,
    bool value_SDC13_E_METER,
    bool value_SDC14_HVD,
    bool value_SDC16_REAR_INTERLOCK,
    bool value_SDC17_TSMS,
    bool value_SDC19_PRECHARGE_COMPLETE,
    bool value_PRECHARGE_INCOMPLETE
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = MAIN_MODULE_FAULT_SYNC_MSG_ID,
        .IDE = false,
        .DLC = MAIN_MODULE_FAULT_SYNC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_SDC1_IMD = (uint64_t)value_SDC1_IMD;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC1_IMD, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_SDC2_BMS = (uint64_t)value_SDC2_BMS;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC2_BMS, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_SDC3_BSPD = (uint64_t)value_SDC3_BSPD;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC3_BSPD, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_SDC4_ECU_OK = (uint64_t)value_SDC4_ECU_OK;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC4_ECU_OK, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_SDC5_BOTS = (uint64_t)value_SDC5_BOTS;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC5_BOTS, 0x1, 4, BSWAP_NONE);
    uint64_t sig_raw_SDC6_INERTIA = (uint64_t)value_SDC6_INERTIA;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC6_INERTIA, 0x1, 5, BSWAP_NONE);
    uint64_t sig_raw_SDC7_COCKPIT_ESTOP = (uint64_t)value_SDC7_COCKPIT_ESTOP;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC7_COCKPIT_ESTOP, 0x1, 6, BSWAP_NONE);
    uint64_t sig_raw_SDC9_FRONT_INTERLOCK = (uint64_t)value_SDC9_FRONT_INTERLOCK;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC9_FRONT_INTERLOCK, 0x1, 7, BSWAP_NONE);
    uint64_t sig_raw_SDC10_RIGHT_ESTOP = (uint64_t)value_SDC10_RIGHT_ESTOP;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC10_RIGHT_ESTOP, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_SDC11_LEFT_ESTOP = (uint64_t)value_SDC11_LEFT_ESTOP;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC11_LEFT_ESTOP, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_SDC12_MSD = (uint64_t)value_SDC12_MSD;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC12_MSD, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_SDC13_E_METER = (uint64_t)value_SDC13_E_METER;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC13_E_METER, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_SDC14_HVD = (uint64_t)value_SDC14_HVD;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC14_HVD, 0x1, 12, BSWAP_NONE);
    uint64_t sig_raw_SDC16_REAR_INTERLOCK = (uint64_t)value_SDC16_REAR_INTERLOCK;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC16_REAR_INTERLOCK, 0x1, 13, BSWAP_NONE);
    uint64_t sig_raw_SDC17_TSMS = (uint64_t)value_SDC17_TSMS;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC17_TSMS, 0x1, 14, BSWAP_NONE);
    uint64_t sig_raw_SDC19_PRECHARGE_COMPLETE = (uint64_t)value_SDC19_PRECHARGE_COMPLETE;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC19_PRECHARGE_COMPLETE, 0x1, 15, BSWAP_NONE);
    uint64_t sig_raw_PRECHARGE_INCOMPLETE = (uint64_t)value_PRECHARGE_INCOMPLETE;
    payload = CAN_pack_raw_signal(payload, sig_raw_PRECHARGE_INCOMPLETE, 0x1, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MAIN_MODULE_FAULT_SYNC_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, MCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN1
static inline void FDCAN1_init_filters() {
    static const uint32_t sid_list[] = {
        PEDALS_MSG_ID,
        START_BUTTON_MSG_ID,
        VCU_SETTINGS_MSG_ID,
        VCU_TORQUE_REQUEST_MSG_ID,
        BMS_VOLTAGES_MSG_ID,
        BL_MAIN_MODULE_START_MSG_ID,
        A_BOX_FAULT_EVENT_MSG_ID,
        DASHBOARD_FAULT_EVENT_MSG_ID,
        PDU_FAULT_EVENT_MSG_ID,
        TORQUE_VECTOR_FAULT_EVENT_MSG_ID,
        A_BOX_FAULT_SYNC_MSG_ID,
        DASHBOARD_FAULT_SYNC_MSG_ID,
        PDU_FAULT_SYNC_MSG_ID,
        TORQUE_VECTOR_FAULT_SYNC_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN1,
        sid_list, 14,
        NULL, 0
    );
}
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    static const uint32_t sid_list[] = {
        INVA_CRIT_MSG_ID,
        INVA_INFO_MSG_ID,
        INVA_ERR_1_MSG_ID,
        INVA_ERR_2_MSG_ID,
        INVA_TEMPS_MSG_ID,
        INVB_CRIT_MSG_ID,
        INVB_INFO_MSG_ID,
        INVB_ERR_1_MSG_ID,
        INVB_ERR_2_MSG_ID,
        INVB_TEMPS_MSG_ID,
        INVC_CRIT_MSG_ID,
        INVC_INFO_MSG_ID,
        INVC_ERR_1_MSG_ID,
        INVC_ERR_2_MSG_ID,
        INVC_TEMPS_MSG_ID,
        INVD_CRIT_MSG_ID,
        INVD_INFO_MSG_ID,
        INVD_ERR_1_MSG_ID,
        INVD_ERR_2_MSG_ID,
        INVD_TEMPS_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN2,
        sid_list, 20,
        NULL, 0
    );
}

#endif // MAIN_MODULE_H
