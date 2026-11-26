#ifndef DASHBOARD_H
#define DASHBOARD_H

/**
 * @file DASHBOARD.h
 * @brief Auto-generated CAN driver for DASHBOARD.
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
#include "can_library/generated/VCAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_DASHBOARD uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN2
#define USE_FDCAN3

void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN3(CanMsgTypeDef_t *msg);

// Scaling Macros
// bms_voltages
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_PACK_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MIN_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MAX_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_AVG_CELL_VOLTAGE = 0.01f;
// brake_pressure
static constexpr float PACK_COEFF_BRAKE_PRESSURE_BRAKE_PRESSURE = 100.0f;
// gps_coordinates
static constexpr float UNPACK_COEFF_GPS_COORDINATES_LATITUDE = 1e-07f;
static constexpr float UNPACK_COEFF_GPS_COORDINATES_LONGITUDE = 1e-07f;
// igbt_temps
static constexpr float UNPACK_COEFF_IGBT_TEMPS_FRONT_RIGHT = 0.1f;
static constexpr float UNPACK_COEFF_IGBT_TEMPS_FRONT_LEFT = 0.1f;
static constexpr float UNPACK_COEFF_IGBT_TEMPS_REAR_LEFT = 0.1f;
static constexpr float UNPACK_COEFF_IGBT_TEMPS_REAR_RIGHT = 0.1f;
// motor_temps
static constexpr float UNPACK_COEFF_MOTOR_TEMPS_FRONT_RIGHT = 0.1f;
static constexpr float UNPACK_COEFF_MOTOR_TEMPS_FRONT_LEFT = 0.1f;
static constexpr float UNPACK_COEFF_MOTOR_TEMPS_REAR_LEFT = 0.1f;
static constexpr float UNPACK_COEFF_MOTOR_TEMPS_REAR_RIGHT = 0.1f;
// pack_analog
static constexpr float UNPACK_COEFF_PACK_ANALOG_PACK_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_PACK_ANALOG_PACK_CURRENT = 0.1f;

// Offset Macros

// Data Structures
typedef struct {
    gps_coordinates_data_t gps_coordinates;
    main_hb_data_t main_hb;
    bms_voltages_data_t bms_voltages;
    bms_temps_data_t bms_temps;
    pack_analog_data_t pack_analog;
    motor_speeds_data_t motor_speeds;
    motor_temps_data_t motor_temps;
    igbt_temps_data_t igbt_temps;
    inva_diagnostics_data_t inva_diagnostics;
    invb_diagnostics_data_t invb_diagnostics;
    invc_diagnostics_data_t invc_diagnostics;
    invd_diagnostics_data_t invd_diagnostics;
    vcu_settings_data_t vcu_settings;
    vcu_torque_request_data_t vcu_torque_request;
    bl_dashboard_start_data_t bl_dashboard_start;
    a_box_fault_event_data_t a_box_fault_event;
    main_module_fault_event_data_t main_module_fault_event;
    pdu_fault_event_data_t pdu_fault_event;
    torque_vector_fault_event_data_t torque_vector_fault_event;
    a_box_fault_sync_data_t a_box_fault_sync;
    main_module_fault_sync_data_t main_module_fault_sync;
    pdu_fault_sync_data_t pdu_fault_sync;
    torque_vector_fault_sync_data_t torque_vector_fault_sync;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void vcu_settings_CALLBACK();
extern void bl_dashboard_start_CALLBACK();
extern void a_box_fault_event_CALLBACK();
extern void main_module_fault_event_CALLBACK();
extern void pdu_fault_event_CALLBACK();
extern void torque_vector_fault_event_CALLBACK();
extern void a_box_fault_sync_CALLBACK();
extern void main_module_fault_sync_CALLBACK();
extern void pdu_fault_sync_CALLBACK();
extern void torque_vector_fault_sync_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN2) {
        switch (id) {
            case MAIN_HB_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_car_state = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.main_hb.car_state = (car_state_t)sig_raw_car_state;

                can_data.main_hb.last_rx = xTaskGetTickCount();
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
            case BMS_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_min_cell_temp = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.bms_temps.min_cell_temp = (uint8_t)sig_raw_min_cell_temp;
                uint64_t sig_raw_max_cell_temp = CAN_unpack_raw_signal(host_data, 0xFF, 8, BSWAP_NONE);
                can_data.bms_temps.max_cell_temp = (uint8_t)sig_raw_max_cell_temp;
                uint64_t sig_raw_avg_cell_temp = CAN_unpack_raw_signal(host_data, 0xFF, 16, BSWAP_NONE);
                can_data.bms_temps.avg_cell_temp = (uint8_t)sig_raw_avg_cell_temp;
                uint64_t sig_raw_min_board_temp = CAN_unpack_raw_signal(host_data, 0xFF, 24, BSWAP_NONE);
                can_data.bms_temps.min_board_temp = (uint8_t)sig_raw_min_board_temp;
                uint64_t sig_raw_max_board_temp = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.bms_temps.max_board_temp = (uint8_t)sig_raw_max_board_temp;
                uint64_t sig_raw_avg_board_temp = CAN_unpack_raw_signal(host_data, 0xFF, 40, BSWAP_NONE);
                can_data.bms_temps.avg_board_temp = (uint8_t)sig_raw_avg_board_temp;

                can_data.bms_temps.last_rx = xTaskGetTickCount();
                break;
            }
            case PACK_ANALOG_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_pack_voltage = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.pack_analog.pack_voltage = (uint16_t)sig_raw_pack_voltage;
                uint64_t sig_raw_pack_current = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.pack_analog.pack_current = (int16_t)CAN_sign_extend_raw(sig_raw_pack_current, 16);

                can_data.pack_analog.last_rx = xTaskGetTickCount();
                break;
            }
            case MOTOR_SPEEDS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_front_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.motor_speeds.front_right = (int16_t)CAN_sign_extend_raw(sig_raw_front_right, 16);
                uint64_t sig_raw_front_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.motor_speeds.front_left = (int16_t)CAN_sign_extend_raw(sig_raw_front_left, 16);
                uint64_t sig_raw_rear_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.motor_speeds.rear_left = (int16_t)CAN_sign_extend_raw(sig_raw_rear_left, 16);
                uint64_t sig_raw_rear_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.motor_speeds.rear_right = (int16_t)CAN_sign_extend_raw(sig_raw_rear_right, 16);

                can_data.motor_speeds.last_rx = xTaskGetTickCount();
                break;
            }
            case MOTOR_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_front_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.motor_temps.front_right = (int16_t)CAN_sign_extend_raw(sig_raw_front_right, 16);
                uint64_t sig_raw_front_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.motor_temps.front_left = (int16_t)CAN_sign_extend_raw(sig_raw_front_left, 16);
                uint64_t sig_raw_rear_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.motor_temps.rear_left = (int16_t)CAN_sign_extend_raw(sig_raw_rear_left, 16);
                uint64_t sig_raw_rear_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.motor_temps.rear_right = (int16_t)CAN_sign_extend_raw(sig_raw_rear_right, 16);

                can_data.motor_temps.last_rx = xTaskGetTickCount();
                break;
            }
            case IGBT_TEMPS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_front_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.igbt_temps.front_right = (int16_t)CAN_sign_extend_raw(sig_raw_front_right, 16);
                uint64_t sig_raw_front_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_NONE);
                can_data.igbt_temps.front_left = (int16_t)CAN_sign_extend_raw(sig_raw_front_left, 16);
                uint64_t sig_raw_rear_left = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_NONE);
                can_data.igbt_temps.rear_left = (int16_t)CAN_sign_extend_raw(sig_raw_rear_left, 16);
                uint64_t sig_raw_rear_right = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_NONE);
                can_data.igbt_temps.rear_right = (int16_t)CAN_sign_extend_raw(sig_raw_rear_right, 16);

                can_data.igbt_temps.last_rx = xTaskGetTickCount();
                break;
            }
            case INVA_DIAGNOSTICS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_diagnostic_number = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.inva_diagnostics.diagnostic_number = (uint32_t)sig_raw_diagnostic_number;
                uint64_t sig_raw_amk_state = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.inva_diagnostics.amk_state = (AMK_state_t)sig_raw_amk_state;
                uint64_t sig_raw_bError = CAN_unpack_raw_signal(host_data, 0x1, 40, BSWAP_NONE);
                can_data.inva_diagnostics.bError = (bool)sig_raw_bError;
                uint64_t sig_raw_InverterOn = CAN_unpack_raw_signal(host_data, 0x1, 41, BSWAP_NONE);
                can_data.inva_diagnostics.InverterOn = (bool)sig_raw_InverterOn;
                uint64_t sig_raw_is_any_stale = CAN_unpack_raw_signal(host_data, 0x1, 42, BSWAP_NONE);
                can_data.inva_diagnostics.is_any_stale = (bool)sig_raw_is_any_stale;

                can_data.inva_diagnostics.last_rx = xTaskGetTickCount();
                break;
            }
            case INVB_DIAGNOSTICS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_diagnostic_number = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.invb_diagnostics.diagnostic_number = (uint32_t)sig_raw_diagnostic_number;
                uint64_t sig_raw_amk_state = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.invb_diagnostics.amk_state = (AMK_state_t)sig_raw_amk_state;
                uint64_t sig_raw_bError = CAN_unpack_raw_signal(host_data, 0x1, 40, BSWAP_NONE);
                can_data.invb_diagnostics.bError = (bool)sig_raw_bError;
                uint64_t sig_raw_InverterOn = CAN_unpack_raw_signal(host_data, 0x1, 41, BSWAP_NONE);
                can_data.invb_diagnostics.InverterOn = (bool)sig_raw_InverterOn;
                uint64_t sig_raw_is_any_stale = CAN_unpack_raw_signal(host_data, 0x1, 42, BSWAP_NONE);
                can_data.invb_diagnostics.is_any_stale = (bool)sig_raw_is_any_stale;

                can_data.invb_diagnostics.last_rx = xTaskGetTickCount();
                break;
            }
            case INVC_DIAGNOSTICS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_diagnostic_number = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.invc_diagnostics.diagnostic_number = (uint32_t)sig_raw_diagnostic_number;
                uint64_t sig_raw_amk_state = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.invc_diagnostics.amk_state = (AMK_state_t)sig_raw_amk_state;
                uint64_t sig_raw_bError = CAN_unpack_raw_signal(host_data, 0x1, 40, BSWAP_NONE);
                can_data.invc_diagnostics.bError = (bool)sig_raw_bError;
                uint64_t sig_raw_InverterOn = CAN_unpack_raw_signal(host_data, 0x1, 41, BSWAP_NONE);
                can_data.invc_diagnostics.InverterOn = (bool)sig_raw_InverterOn;
                uint64_t sig_raw_is_any_stale = CAN_unpack_raw_signal(host_data, 0x1, 42, BSWAP_NONE);
                can_data.invc_diagnostics.is_any_stale = (bool)sig_raw_is_any_stale;

                can_data.invc_diagnostics.last_rx = xTaskGetTickCount();
                break;
            }
            case INVD_DIAGNOSTICS_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_diagnostic_number = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.invd_diagnostics.diagnostic_number = (uint32_t)sig_raw_diagnostic_number;
                uint64_t sig_raw_amk_state = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.invd_diagnostics.amk_state = (AMK_state_t)sig_raw_amk_state;
                uint64_t sig_raw_bError = CAN_unpack_raw_signal(host_data, 0x1, 40, BSWAP_NONE);
                can_data.invd_diagnostics.bError = (bool)sig_raw_bError;
                uint64_t sig_raw_InverterOn = CAN_unpack_raw_signal(host_data, 0x1, 41, BSWAP_NONE);
                can_data.invd_diagnostics.InverterOn = (bool)sig_raw_InverterOn;
                uint64_t sig_raw_is_any_stale = CAN_unpack_raw_signal(host_data, 0x1, 42, BSWAP_NONE);
                can_data.invd_diagnostics.is_any_stale = (bool)sig_raw_is_any_stale;

                can_data.invd_diagnostics.last_rx = xTaskGetTickCount();
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
                vcu_settings_CALLBACK();
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
            case BL_DASHBOARD_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_dashboard_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_dashboard_start.last_rx = xTaskGetTickCount();
                bl_dashboard_start_CALLBACK();
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
    else if (bus == FDCAN3) {
        switch (id) {
            case GPS_COORDINATES_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_latitude = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.gps_coordinates.latitude = (int32_t)CAN_sign_extend_raw(sig_raw_latitude, 32);
                uint64_t sig_raw_longitude = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 32, BSWAP_NONE);
                can_data.gps_coordinates.longitude = (int32_t)CAN_sign_extend_raw(sig_raw_longitude, 32);

                can_data.gps_coordinates.last_rx = xTaskGetTickCount();
                break;
            }
            default:
                break;
        }
    }
}

// Stale Checking
static bool is_gps_coordinates_stale() {
    return (xTaskGetTickCount() - can_data.gps_coordinates.last_rx) > GPS_COORDINATES_STALE_TIMEOUT_MS;
}
static bool is_main_hb_stale() {
    return (xTaskGetTickCount() - can_data.main_hb.last_rx) > MAIN_HB_STALE_TIMEOUT_MS;
}
static bool is_bms_voltages_stale() {
    return (xTaskGetTickCount() - can_data.bms_voltages.last_rx) > BMS_VOLTAGES_STALE_TIMEOUT_MS;
}
static bool is_bms_temps_stale() {
    return (xTaskGetTickCount() - can_data.bms_temps.last_rx) > BMS_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_pack_analog_stale() {
    return (xTaskGetTickCount() - can_data.pack_analog.last_rx) > PACK_ANALOG_STALE_TIMEOUT_MS;
}
static bool is_motor_speeds_stale() {
    return (xTaskGetTickCount() - can_data.motor_speeds.last_rx) > MOTOR_SPEEDS_STALE_TIMEOUT_MS;
}
static bool is_motor_temps_stale() {
    return (xTaskGetTickCount() - can_data.motor_temps.last_rx) > MOTOR_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_igbt_temps_stale() {
    return (xTaskGetTickCount() - can_data.igbt_temps.last_rx) > IGBT_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_inva_diagnostics_stale() {
    return (xTaskGetTickCount() - can_data.inva_diagnostics.last_rx) > INVA_DIAGNOSTICS_STALE_TIMEOUT_MS;
}
static bool is_invb_diagnostics_stale() {
    return (xTaskGetTickCount() - can_data.invb_diagnostics.last_rx) > INVB_DIAGNOSTICS_STALE_TIMEOUT_MS;
}
static bool is_invc_diagnostics_stale() {
    return (xTaskGetTickCount() - can_data.invc_diagnostics.last_rx) > INVC_DIAGNOSTICS_STALE_TIMEOUT_MS;
}
static bool is_invd_diagnostics_stale() {
    return (xTaskGetTickCount() - can_data.invd_diagnostics.last_rx) > INVD_DIAGNOSTICS_STALE_TIMEOUT_MS;
}
static bool is_vcu_settings_stale() {
    return (xTaskGetTickCount() - can_data.vcu_settings.last_rx) > VCU_SETTINGS_STALE_TIMEOUT_MS;
}
static bool is_vcu_torque_request_stale() {
    return (xTaskGetTickCount() - can_data.vcu_torque_request.last_rx) > VCU_TORQUE_REQUEST_STALE_TIMEOUT_MS;
}
static bool is_a_box_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.a_box_fault_sync.last_rx) > A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS;
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
    can_data.gps_coordinates.last_rx = (uint32_t) -(GPS_COORDINATES_STALE_TIMEOUT_MS + 1);
    can_data.gps_coordinates.is_stale = is_gps_coordinates_stale;
    can_data.main_hb.last_rx = (uint32_t) -(MAIN_HB_STALE_TIMEOUT_MS + 1);
    can_data.main_hb.is_stale = is_main_hb_stale;
    can_data.bms_voltages.last_rx = (uint32_t) -(BMS_VOLTAGES_STALE_TIMEOUT_MS + 1);
    can_data.bms_voltages.is_stale = is_bms_voltages_stale;
    can_data.bms_temps.last_rx = (uint32_t) -(BMS_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.bms_temps.is_stale = is_bms_temps_stale;
    can_data.pack_analog.last_rx = (uint32_t) -(PACK_ANALOG_STALE_TIMEOUT_MS + 1);
    can_data.pack_analog.is_stale = is_pack_analog_stale;
    can_data.motor_speeds.last_rx = (uint32_t) -(MOTOR_SPEEDS_STALE_TIMEOUT_MS + 1);
    can_data.motor_speeds.is_stale = is_motor_speeds_stale;
    can_data.motor_temps.last_rx = (uint32_t) -(MOTOR_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.motor_temps.is_stale = is_motor_temps_stale;
    can_data.igbt_temps.last_rx = (uint32_t) -(IGBT_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.igbt_temps.is_stale = is_igbt_temps_stale;
    can_data.inva_diagnostics.last_rx = (uint32_t) -(INVA_DIAGNOSTICS_STALE_TIMEOUT_MS + 1);
    can_data.inva_diagnostics.is_stale = is_inva_diagnostics_stale;
    can_data.invb_diagnostics.last_rx = (uint32_t) -(INVB_DIAGNOSTICS_STALE_TIMEOUT_MS + 1);
    can_data.invb_diagnostics.is_stale = is_invb_diagnostics_stale;
    can_data.invc_diagnostics.last_rx = (uint32_t) -(INVC_DIAGNOSTICS_STALE_TIMEOUT_MS + 1);
    can_data.invc_diagnostics.is_stale = is_invc_diagnostics_stale;
    can_data.invd_diagnostics.last_rx = (uint32_t) -(INVD_DIAGNOSTICS_STALE_TIMEOUT_MS + 1);
    can_data.invd_diagnostics.is_stale = is_invd_diagnostics_stale;
    can_data.vcu_settings.last_rx = (uint32_t) -(VCU_SETTINGS_STALE_TIMEOUT_MS + 1);
    can_data.vcu_settings.is_stale = is_vcu_settings_stale;
    can_data.vcu_torque_request.last_rx = (uint32_t) -(VCU_TORQUE_REQUEST_STALE_TIMEOUT_MS + 1);
    can_data.vcu_torque_request.is_stale = is_vcu_torque_request_stale;
    can_data.a_box_fault_sync.last_rx = (uint32_t) -(A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.a_box_fault_sync.is_stale = is_a_box_fault_sync_stale;
    can_data.main_module_fault_sync.last_rx = (uint32_t) -(MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.main_module_fault_sync.is_stale = is_main_module_fault_sync_stale;
    can_data.pdu_fault_sync.last_rx = (uint32_t) -(PDU_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.pdu_fault_sync.is_stale = is_pdu_fault_sync_stale;
    can_data.torque_vector_fault_sync.last_rx = (uint32_t) -(TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.torque_vector_fault_sync.is_stale = is_torque_vector_fault_sync_stale;
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_dash_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = DASH_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = DASH_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASH_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dash_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = DASH_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = DASH_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASH_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dash_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = DASH_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = DASH_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASH_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_brake_pressure(
    uint16_t value_brake_pressure
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = BRAKE_PRESSURE_MSG_ID,
        .IDE = false,
        .DLC = BRAKE_PRESSURE_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_brake_pressure = (uint64_t)value_brake_pressure;
    payload = CAN_pack_raw_signal(payload, sig_raw_brake_pressure, 0xFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, BRAKE_PRESSURE_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_mark_data(
    uint32_t value_os_tick,
    uint16_t value_mark_index
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN3,
        .StdId = MARK_DATA_MSG_ID,
        .IDE = false,
        .DLC = MARK_DATA_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_os_tick = (uint64_t)value_os_tick;
    payload = CAN_pack_raw_signal(payload, sig_raw_os_tick, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_mark_index = (uint64_t)value_mark_index;
    payload = CAN_pack_raw_signal(payload, sig_raw_mark_index, 0xFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, MARK_DATA_DLC);

    CAN_enqueue_tx_FDCAN3(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dash_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = DASH_INIT_MSG_ID,
        .IDE = false,
        .DLC = DASH_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASH_INIT_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pedals(
    uint8_t value_throttle,
    uint8_t value_regen,
    uint8_t value_brake
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = PEDALS_MSG_ID,
        .IDE = false,
        .DLC = PEDALS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_throttle = (uint64_t)value_throttle;
    payload = CAN_pack_raw_signal(payload, sig_raw_throttle, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_regen = (uint64_t)value_regen;
    payload = CAN_pack_raw_signal(payload, sig_raw_regen, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_brake = (uint64_t)value_brake;
    payload = CAN_pack_raw_signal(payload, sig_raw_brake, 0xFF, 16, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PEDALS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_start_button(
    bool value_is_pressed
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = START_BUTTON_MSG_ID,
        .IDE = false,
        .DLC = START_BUTTON_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_is_pressed = (uint64_t)value_is_pressed;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_pressed, 0x1, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, START_BUTTON_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_vcu_driver_request(
    vcu_mode_t value_vcu_mode,
    uint8_t value_lateral_gain,
    uint8_t value_longitudinal_gain,
    uint8_t value_electronic_brake_bias,
    bool value_is_regen_enabled,
    bool value_is_tv_enabled
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = VCU_DRIVER_REQUEST_MSG_ID,
        .IDE = false,
        .DLC = VCU_DRIVER_REQUEST_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_vcu_mode = (uint64_t)value_vcu_mode;
    payload = CAN_pack_raw_signal(payload, sig_raw_vcu_mode, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_lateral_gain = (uint64_t)value_lateral_gain;
    payload = CAN_pack_raw_signal(payload, sig_raw_lateral_gain, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_longitudinal_gain = (uint64_t)value_longitudinal_gain;
    payload = CAN_pack_raw_signal(payload, sig_raw_longitudinal_gain, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_electronic_brake_bias = (uint64_t)value_electronic_brake_bias;
    payload = CAN_pack_raw_signal(payload, sig_raw_electronic_brake_bias, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_is_regen_enabled = (uint64_t)value_is_regen_enabled;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_regen_enabled, 0x1, 32, BSWAP_NONE);
    uint64_t sig_raw_is_tv_enabled = (uint64_t)value_is_tv_enabled;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_tv_enabled, 0x1, 33, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, VCU_DRIVER_REQUEST_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dash_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = DASH_VERSION_MSG_ID,
        .IDE = false,
        .DLC = DASH_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASH_VERSION_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_LWS_Config(
    uint8_t value_CCW
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = LWS_CONFIG_MSG_ID,
        .IDE = false,
        .DLC = LWS_CONFIG_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_CCW = (uint64_t)value_CCW;
    payload = CAN_pack_raw_signal(payload, sig_raw_CCW, 0x7, 0, BSWAP_NONE);
    uint64_t sig_raw_reserved1 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved1, 0x1F, 3, BSWAP_NONE);
    uint64_t sig_raw_reserved2 = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved2, 0xFF, 8, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, LWS_CONFIG_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dashboard_fault_event(
    fault_id_t value_idx,
    uint16_t value_val,
    bool value_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = DASHBOARD_FAULT_EVENT_MSG_ID,
        .IDE = false,
        .DLC = DASHBOARD_FAULT_EVENT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_idx = (uint64_t)value_idx;
    payload = CAN_pack_raw_signal(payload, sig_raw_idx, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_val = (uint64_t)value_val;
    payload = CAN_pack_raw_signal(payload, sig_raw_val, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_state = (uint64_t)value_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_state, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASHBOARD_FAULT_EVENT_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_dashboard_fault_sync(
    bool value_APPS_wiring_t1,
    bool value_APPS_wiring_t2,
    bool value_APPS_IMPLAUSIBLE,
    bool value_APPS_BRAKE,
    bool value_BSE
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = DASHBOARD_FAULT_SYNC_MSG_ID,
        .IDE = false,
        .DLC = DASHBOARD_FAULT_SYNC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_APPS_wiring_t1 = (uint64_t)value_APPS_wiring_t1;
    payload = CAN_pack_raw_signal(payload, sig_raw_APPS_wiring_t1, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_APPS_wiring_t2 = (uint64_t)value_APPS_wiring_t2;
    payload = CAN_pack_raw_signal(payload, sig_raw_APPS_wiring_t2, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_APPS_IMPLAUSIBLE = (uint64_t)value_APPS_IMPLAUSIBLE;
    payload = CAN_pack_raw_signal(payload, sig_raw_APPS_IMPLAUSIBLE, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_APPS_BRAKE = (uint64_t)value_APPS_BRAKE;
    payload = CAN_pack_raw_signal(payload, sig_raw_APPS_BRAKE, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_BSE = (uint64_t)value_BSE;
    payload = CAN_pack_raw_signal(payload, sig_raw_BSE, 0x1, 4, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, DASHBOARD_FAULT_SYNC_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, SCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    static const uint32_t sid_list[] = {
        MAIN_HB_MSG_ID,
        BMS_VOLTAGES_MSG_ID,
        BMS_TEMPS_MSG_ID,
        PACK_ANALOG_MSG_ID,
        MOTOR_SPEEDS_MSG_ID,
        MOTOR_TEMPS_MSG_ID,
        IGBT_TEMPS_MSG_ID,
        INVA_DIAGNOSTICS_MSG_ID,
        INVB_DIAGNOSTICS_MSG_ID,
        INVC_DIAGNOSTICS_MSG_ID,
        INVD_DIAGNOSTICS_MSG_ID,
        VCU_SETTINGS_MSG_ID,
        VCU_TORQUE_REQUEST_MSG_ID,
        BL_DASHBOARD_START_MSG_ID,
        A_BOX_FAULT_EVENT_MSG_ID,
        MAIN_MODULE_FAULT_EVENT_MSG_ID,
        PDU_FAULT_EVENT_MSG_ID,
        TORQUE_VECTOR_FAULT_EVENT_MSG_ID,
        A_BOX_FAULT_SYNC_MSG_ID,
        MAIN_MODULE_FAULT_SYNC_MSG_ID,
        PDU_FAULT_SYNC_MSG_ID,
        TORQUE_VECTOR_FAULT_SYNC_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN2,
        sid_list, 22,
        NULL, 0
    );
}
// CAN filter setup for FDCAN3
static inline void FDCAN3_init_filters() {
    static const uint32_t sid_list[] = {
        GPS_COORDINATES_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN3,
        sid_list, 1,
        NULL, 0
    );
}

#endif // DASHBOARD_H
