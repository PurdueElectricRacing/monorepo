#ifndef TORQUE_VECTOR_H
#define TORQUE_VECTOR_H

/**
 * @file TORQUE_VECTOR.h
 * @brief Auto-generated CAN driver for TORQUE_VECTOR.
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
#error "CAN_NODE_TORQUE_VECTOR uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN1
#define USE_FDCAN2

void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);

// Scaling Macros
// IMU_acceleration
static constexpr float PACK_COEFF_IMU_ACCELERATION_X_AXIS = 100.0f;
static constexpr float PACK_COEFF_IMU_ACCELERATION_Y_AXIS = 100.0f;
static constexpr float PACK_COEFF_IMU_ACCELERATION_Z_AXIS = 100.0f;
static constexpr float PACK_COEFF_IMU_ACCELERATION_TEMPERATURE = 10.0f;
// IMU_angular_rate
static constexpr float PACK_COEFF_IMU_ANGULAR_RATE_X_AXIS = 10.0f;
static constexpr float PACK_COEFF_IMU_ANGULAR_RATE_Y_AXIS = 10.0f;
static constexpr float PACK_COEFF_IMU_ANGULAR_RATE_Z_AXIS = 10.0f;
// IZZE_acceleration
static constexpr float UNPACK_COEFF_IZZE_ACCELERATION_X_AXIS = 0.01f;
static constexpr float UNPACK_COEFF_IZZE_ACCELERATION_Y_AXIS = 0.01f;
static constexpr float UNPACK_COEFF_IZZE_ACCELERATION_Z_AXIS = 0.01f;
static constexpr float UNPACK_COEFF_IZZE_ACCELERATION_TEMPERATURE = 0.1f;
// IZZE_angular_rate
static constexpr float UNPACK_COEFF_IZZE_ANGULAR_RATE_X_AXIS = 0.1f;
static constexpr float UNPACK_COEFF_IZZE_ANGULAR_RATE_Y_AXIS = 0.1f;
static constexpr float UNPACK_COEFF_IZZE_ANGULAR_RATE_Z_AXIS = 0.1f;
// LWS_Standard
static constexpr float UNPACK_COEFF_LWS_STANDARD_LWS_ANGLE = 0.1f;
static constexpr float UNPACK_COEFF_LWS_STANDARD_LWS_SPEED = 4.0f;
// bms_voltages
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_PACK_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MIN_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_MAX_CELL_VOLTAGE = 0.01f;
static constexpr float UNPACK_COEFF_BMS_VOLTAGES_AVG_CELL_VOLTAGE = 0.01f;
// gps_coordinates
static constexpr float PACK_COEFF_GPS_COORDINATES_LATITUDE = 10000000.0f;
static constexpr float PACK_COEFF_GPS_COORDINATES_LONGITUDE = 10000000.0f;
// gps_speed_heading
static constexpr float PACK_COEFF_GPS_SPEED_HEADING_GPS_SPEED = 1000.0f;
static constexpr float PACK_COEFF_GPS_SPEED_HEADING_GPS_HEADING = 100000.0f;
// gps_velocity
static constexpr float PACK_COEFF_GPS_VELOCITY_GPS_VEL_N = 1000.0f;
static constexpr float PACK_COEFF_GPS_VELOCITY_GPS_VEL_E = 1000.0f;
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
    IZZE_angular_rate_data_t IZZE_angular_rate;
    IZZE_acceleration_data_t IZZE_acceleration;
    pedals_data_t pedals;
    LWS_Standard_data_t LWS_Standard;
    bms_voltages_data_t bms_voltages;
    pack_analog_data_t pack_analog;
    motor_speeds_data_t motor_speeds;
    motor_temps_data_t motor_temps;
    igbt_temps_data_t igbt_temps;
    vcu_driver_request_data_t vcu_driver_request;
    bl_torque_vector_start_data_t bl_torque_vector_start;
    a_box_fault_event_data_t a_box_fault_event;
    dashboard_fault_event_data_t dashboard_fault_event;
    main_module_fault_event_data_t main_module_fault_event;
    pdu_fault_event_data_t pdu_fault_event;
    a_box_fault_sync_data_t a_box_fault_sync;
    dashboard_fault_sync_data_t dashboard_fault_sync;
    main_module_fault_sync_data_t main_module_fault_sync;
    pdu_fault_sync_data_t pdu_fault_sync;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void IZZE_angular_rate_CALLBACK();
extern void IZZE_acceleration_CALLBACK();
extern void vcu_driver_request_CALLBACK();
extern void bl_torque_vector_start_CALLBACK();
extern void a_box_fault_event_CALLBACK();
extern void dashboard_fault_event_CALLBACK();
extern void main_module_fault_event_CALLBACK();
extern void pdu_fault_event_CALLBACK();
extern void a_box_fault_sync_CALLBACK();
extern void dashboard_fault_sync_CALLBACK();
extern void main_module_fault_sync_CALLBACK();
extern void pdu_fault_sync_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN2) {
        switch (id) {
            case IZZE_ANGULAR_RATE_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_X_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_16);
                can_data.IZZE_angular_rate.X_axis = (int16_t)CAN_sign_extend_raw(sig_raw_X_axis, 16);
                uint64_t sig_raw_Y_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_16);
                can_data.IZZE_angular_rate.Y_axis = (int16_t)CAN_sign_extend_raw(sig_raw_Y_axis, 16);
                uint64_t sig_raw_Z_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_16);
                can_data.IZZE_angular_rate.Z_axis = (int16_t)CAN_sign_extend_raw(sig_raw_Z_axis, 16);
                uint64_t sig_raw_reserved = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_16);
                can_data.IZZE_angular_rate.reserved = (uint16_t)sig_raw_reserved;

                can_data.IZZE_angular_rate.last_rx = xTaskGetTickCount();
                IZZE_angular_rate_CALLBACK();
                break;
            }
            case IZZE_ACCELERATION_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_X_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_16);
                can_data.IZZE_acceleration.X_axis = (int16_t)CAN_sign_extend_raw(sig_raw_X_axis, 16);
                uint64_t sig_raw_Y_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 16, BSWAP_16);
                can_data.IZZE_acceleration.Y_axis = (int16_t)CAN_sign_extend_raw(sig_raw_Y_axis, 16);
                uint64_t sig_raw_Z_axis = CAN_unpack_raw_signal(host_data, 0xFFFF, 32, BSWAP_16);
                can_data.IZZE_acceleration.Z_axis = (int16_t)CAN_sign_extend_raw(sig_raw_Z_axis, 16);
                uint64_t sig_raw_temperature = CAN_unpack_raw_signal(host_data, 0xFFFF, 48, BSWAP_16);
                can_data.IZZE_acceleration.temperature = (int16_t)CAN_sign_extend_raw(sig_raw_temperature, 16);

                can_data.IZZE_acceleration.last_rx = xTaskGetTickCount();
                IZZE_acceleration_CALLBACK();
                break;
            }
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
            case LWS_STANDARD_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_LWS_ANGLE = CAN_unpack_raw_signal(host_data, 0xFFFF, 0, BSWAP_NONE);
                can_data.LWS_Standard.LWS_ANGLE = (int16_t)CAN_sign_extend_raw(sig_raw_LWS_ANGLE, 16);
                uint64_t sig_raw_LWS_SPEED = CAN_unpack_raw_signal(host_data, 0xFF, 16, BSWAP_NONE);
                can_data.LWS_Standard.LWS_SPEED = (uint8_t)sig_raw_LWS_SPEED;
                uint64_t sig_raw_OK = CAN_unpack_raw_signal(host_data, 0x1, 24, BSWAP_NONE);
                can_data.LWS_Standard.OK = (bool)sig_raw_OK;
                uint64_t sig_raw_CAL = CAN_unpack_raw_signal(host_data, 0x1, 25, BSWAP_NONE);
                can_data.LWS_Standard.CAL = (bool)sig_raw_CAL;
                uint64_t sig_raw_TRIM = CAN_unpack_raw_signal(host_data, 0x1, 26, BSWAP_NONE);
                can_data.LWS_Standard.TRIM = (bool)sig_raw_TRIM;
                uint64_t sig_raw_reserved1 = CAN_unpack_raw_signal(host_data, 0x1F, 27, BSWAP_NONE);
                can_data.LWS_Standard.reserved1 = (uint8_t)sig_raw_reserved1;
                uint64_t sig_raw_reserved2 = CAN_unpack_raw_signal(host_data, 0xFF, 32, BSWAP_NONE);
                can_data.LWS_Standard.reserved2 = (uint8_t)sig_raw_reserved2;

                can_data.LWS_Standard.last_rx = xTaskGetTickCount();
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
            case VCU_DRIVER_REQUEST_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_vcu_mode = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.vcu_driver_request.vcu_mode = (vcu_mode_t)sig_raw_vcu_mode;
                uint64_t sig_raw_lateral_gain = CAN_unpack_raw_signal(host_data, 0xFF, 8, BSWAP_NONE);
                can_data.vcu_driver_request.lateral_gain = (uint8_t)sig_raw_lateral_gain;
                uint64_t sig_raw_longitudinal_gain = CAN_unpack_raw_signal(host_data, 0xFF, 16, BSWAP_NONE);
                can_data.vcu_driver_request.longitudinal_gain = (uint8_t)sig_raw_longitudinal_gain;
                uint64_t sig_raw_electronic_brake_bias = CAN_unpack_raw_signal(host_data, 0xFF, 24, BSWAP_NONE);
                can_data.vcu_driver_request.electronic_brake_bias = (uint8_t)sig_raw_electronic_brake_bias;
                uint64_t sig_raw_is_regen_enabled = CAN_unpack_raw_signal(host_data, 0x1, 32, BSWAP_NONE);
                can_data.vcu_driver_request.is_regen_enabled = (bool)sig_raw_is_regen_enabled;
                uint64_t sig_raw_is_tv_enabled = CAN_unpack_raw_signal(host_data, 0x1, 33, BSWAP_NONE);
                can_data.vcu_driver_request.is_tv_enabled = (bool)sig_raw_is_tv_enabled;

                can_data.vcu_driver_request.last_rx = xTaskGetTickCount();
                vcu_driver_request_CALLBACK();
                break;
            }
            case BL_TORQUE_VECTOR_START_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_image_size_bytes = CAN_unpack_raw_signal(host_data, 0xFFFFFFFF, 0, BSWAP_NONE);
                can_data.bl_torque_vector_start.image_size_bytes = (uint32_t)sig_raw_image_size_bytes;

                can_data.bl_torque_vector_start.last_rx = xTaskGetTickCount();
                bl_torque_vector_start_CALLBACK();
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
            default:
                break;
        }
    }
}

// Stale Checking
static bool is_IZZE_angular_rate_stale() {
    return (xTaskGetTickCount() - can_data.IZZE_angular_rate.last_rx) > IZZE_ANGULAR_RATE_STALE_TIMEOUT_MS;
}
static bool is_IZZE_acceleration_stale() {
    return (xTaskGetTickCount() - can_data.IZZE_acceleration.last_rx) > IZZE_ACCELERATION_STALE_TIMEOUT_MS;
}
static bool is_pedals_stale() {
    return (xTaskGetTickCount() - can_data.pedals.last_rx) > PEDALS_STALE_TIMEOUT_MS;
}
static bool is_LWS_Standard_stale() {
    return (xTaskGetTickCount() - can_data.LWS_Standard.last_rx) > LWS_STANDARD_STALE_TIMEOUT_MS;
}
static bool is_bms_voltages_stale() {
    return (xTaskGetTickCount() - can_data.bms_voltages.last_rx) > BMS_VOLTAGES_STALE_TIMEOUT_MS;
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
static bool is_a_box_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.a_box_fault_sync.last_rx) > A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS;
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

// Data initialization
static inline void CAN_init_data(void) {
    memset((void *)&can_data, 0, sizeof(can_data));
    // Underflow the last_rx timers to ensure messages are stale until first rx
    // Setup the stale function pointers
    can_data.IZZE_angular_rate.last_rx = (uint32_t) -(IZZE_ANGULAR_RATE_STALE_TIMEOUT_MS + 1);
    can_data.IZZE_angular_rate.is_stale = is_IZZE_angular_rate_stale;
    can_data.IZZE_acceleration.last_rx = (uint32_t) -(IZZE_ACCELERATION_STALE_TIMEOUT_MS + 1);
    can_data.IZZE_acceleration.is_stale = is_IZZE_acceleration_stale;
    can_data.pedals.last_rx = (uint32_t) -(PEDALS_STALE_TIMEOUT_MS + 1);
    can_data.pedals.is_stale = is_pedals_stale;
    can_data.LWS_Standard.last_rx = (uint32_t) -(LWS_STANDARD_STALE_TIMEOUT_MS + 1);
    can_data.LWS_Standard.is_stale = is_LWS_Standard_stale;
    can_data.bms_voltages.last_rx = (uint32_t) -(BMS_VOLTAGES_STALE_TIMEOUT_MS + 1);
    can_data.bms_voltages.is_stale = is_bms_voltages_stale;
    can_data.pack_analog.last_rx = (uint32_t) -(PACK_ANALOG_STALE_TIMEOUT_MS + 1);
    can_data.pack_analog.is_stale = is_pack_analog_stale;
    can_data.motor_speeds.last_rx = (uint32_t) -(MOTOR_SPEEDS_STALE_TIMEOUT_MS + 1);
    can_data.motor_speeds.is_stale = is_motor_speeds_stale;
    can_data.motor_temps.last_rx = (uint32_t) -(MOTOR_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.motor_temps.is_stale = is_motor_temps_stale;
    can_data.igbt_temps.last_rx = (uint32_t) -(IGBT_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.igbt_temps.is_stale = is_igbt_temps_stale;
    can_data.a_box_fault_sync.last_rx = (uint32_t) -(A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.a_box_fault_sync.is_stale = is_a_box_fault_sync_stale;
    can_data.dashboard_fault_sync.last_rx = (uint32_t) -(DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.dashboard_fault_sync.is_stale = is_dashboard_fault_sync_stale;
    can_data.main_module_fault_sync.last_rx = (uint32_t) -(MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.main_module_fault_sync.is_stale = is_main_module_fault_sync_stale;
    can_data.pdu_fault_sync.last_rx = (uint32_t) -(PDU_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.pdu_fault_sync.is_stale = is_pdu_fault_sync_stale;
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_gps_velocity(
    int32_t value_gps_vel_n,
    int32_t value_gps_vel_e
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = GPS_VELOCITY_MSG_ID,
        .IDE = false,
        .DLC = GPS_VELOCITY_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_gps_vel_n = (uint64_t)value_gps_vel_n;
    payload = CAN_pack_raw_signal(payload, sig_raw_gps_vel_n, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_gps_vel_e = (uint64_t)value_gps_vel_e;
    payload = CAN_pack_raw_signal(payload, sig_raw_gps_vel_e, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, GPS_VELOCITY_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_gps_speed_heading(
    int32_t value_gps_speed,
    int32_t value_gps_heading
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = GPS_SPEED_HEADING_MSG_ID,
        .IDE = false,
        .DLC = GPS_SPEED_HEADING_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_gps_speed = (uint64_t)value_gps_speed;
    payload = CAN_pack_raw_signal(payload, sig_raw_gps_speed, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_gps_heading = (uint64_t)value_gps_heading;
    payload = CAN_pack_raw_signal(payload, sig_raw_gps_heading, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, GPS_SPEED_HEADING_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_gps_coordinates(
    int32_t value_latitude,
    int32_t value_longitude
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = GPS_COORDINATES_MSG_ID,
        .IDE = false,
        .DLC = GPS_COORDINATES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_latitude = (uint64_t)value_latitude;
    payload = CAN_pack_raw_signal(payload, sig_raw_latitude, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_longitude = (uint64_t)value_longitude;
    payload = CAN_pack_raw_signal(payload, sig_raw_longitude, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, GPS_COORDINATES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_IMU_angular_rate(
    int16_t value_X_axis,
    int16_t value_Y_axis,
    int16_t value_Z_axis
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = IMU_ANGULAR_RATE_MSG_ID,
        .IDE = false,
        .DLC = IMU_ANGULAR_RATE_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_X_axis = (uint64_t)value_X_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_X_axis, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_Y_axis = (uint64_t)value_Y_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_Y_axis, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_Z_axis = (uint64_t)value_Z_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_Z_axis, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_reserved = 0;
    payload = CAN_pack_raw_signal(payload, sig_raw_reserved, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IMU_ANGULAR_RATE_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_IMU_acceleration(
    int16_t value_X_axis,
    int16_t value_Y_axis,
    int16_t value_Z_axis,
    int16_t value_temperature
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = IMU_ACCELERATION_MSG_ID,
        .IDE = false,
        .DLC = IMU_ACCELERATION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_X_axis = (uint64_t)value_X_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_X_axis, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_Y_axis = (uint64_t)value_Y_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_Y_axis, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_Z_axis = (uint64_t)value_Z_axis;
    payload = CAN_pack_raw_signal(payload, sig_raw_Z_axis, 0xFFFF, 32, BSWAP_NONE);
    uint64_t sig_raw_temperature = (uint64_t)value_temperature;
    payload = CAN_pack_raw_signal(payload, sig_raw_temperature, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IMU_ACCELERATION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = TORQUE_VECTOR_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = TORQUE_VECTOR_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = TORQUE_VECTOR_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_tv_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = TV_INIT_MSG_ID,
        .IDE = false,
        .DLC = TV_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TV_INIT_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_gps_time(
    uint8_t value_year,
    uint8_t value_month,
    uint8_t value_day,
    uint8_t value_hour,
    uint8_t value_minute,
    uint8_t value_second,
    uint16_t value_millisecond
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = GPS_TIME_MSG_ID,
        .IDE = false,
        .DLC = GPS_TIME_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_year = (uint64_t)value_year;
    payload = CAN_pack_raw_signal(payload, sig_raw_year, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_month = (uint64_t)value_month;
    payload = CAN_pack_raw_signal(payload, sig_raw_month, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_day = (uint64_t)value_day;
    payload = CAN_pack_raw_signal(payload, sig_raw_day, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_hour = (uint64_t)value_hour;
    payload = CAN_pack_raw_signal(payload, sig_raw_hour, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_minute = (uint64_t)value_minute;
    payload = CAN_pack_raw_signal(payload, sig_raw_minute, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_second = (uint64_t)value_second;
    payload = CAN_pack_raw_signal(payload, sig_raw_second, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_millisecond = (uint64_t)value_millisecond;
    payload = CAN_pack_raw_signal(payload, sig_raw_millisecond, 0xFFFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, GPS_TIME_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_vcu_settings(
    vcu_mode_t value_vcu_mode,
    uint8_t value_lateral_gain,
    uint8_t value_longitudinal_gain,
    uint8_t value_electronic_brake_bias,
    bool value_is_regen_enabled,
    bool value_is_tv_enabled
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = VCU_SETTINGS_MSG_ID,
        .IDE = false,
        .DLC = VCU_SETTINGS_DLC,
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

    CAN_store_payload_u64(outgoing.Data, payload, VCU_SETTINGS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_vcu_torque_request(
    int16_t value_front_right,
    int16_t value_front_left,
    int16_t value_rear_left,
    int16_t value_rear_right
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = VCU_TORQUE_REQUEST_MSG_ID,
        .IDE = false,
        .DLC = VCU_TORQUE_REQUEST_DLC,
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

    CAN_store_payload_u64(outgoing.Data, payload, VCU_TORQUE_REQUEST_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_version(
    uint32_t value_git_hash,
    uint32_t value_bootloader_git_hash,
    bool value_bootloadable
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = TORQUE_VECTOR_VERSION_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_bootloader_git_hash = (uint64_t)value_bootloader_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloader_git_hash, 0xFFFFFFF, 28, BSWAP_NONE);
    uint64_t sig_raw_bootloadable = (uint64_t)value_bootloadable;
    payload = CAN_pack_raw_signal(payload, sig_raw_bootloadable, 0x1, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_VERSION_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_fault_event(
    fault_id_t value_idx,
    uint16_t value_val,
    bool value_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = TORQUE_VECTOR_FAULT_EVENT_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_FAULT_EVENT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_idx = (uint64_t)value_idx;
    payload = CAN_pack_raw_signal(payload, sig_raw_idx, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_val = (uint64_t)value_val;
    payload = CAN_pack_raw_signal(payload, sig_raw_val, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_state = (uint64_t)value_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_state, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_FAULT_EVENT_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_torque_vector_fault_sync(
    bool value_GPS_INVALID_FIX,
    bool value_GPS_WEAK_FIX,
    bool value_GPS_INVALID_UTC
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = TORQUE_VECTOR_FAULT_SYNC_MSG_ID,
        .IDE = false,
        .DLC = TORQUE_VECTOR_FAULT_SYNC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_GPS_INVALID_FIX = (uint64_t)value_GPS_INVALID_FIX;
    payload = CAN_pack_raw_signal(payload, sig_raw_GPS_INVALID_FIX, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_GPS_WEAK_FIX = (uint64_t)value_GPS_WEAK_FIX;
    payload = CAN_pack_raw_signal(payload, sig_raw_GPS_WEAK_FIX, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_GPS_INVALID_UTC = (uint64_t)value_GPS_INVALID_UTC;
    payload = CAN_pack_raw_signal(payload, sig_raw_GPS_INVALID_UTC, 0x1, 2, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, TORQUE_VECTOR_FAULT_SYNC_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN1, SCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN1
static inline void FDCAN1_init_filters() {
    // No filters configured
    (void)0;
}
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    static const uint32_t sid_list[] = {
        IZZE_ANGULAR_RATE_MSG_ID,
        IZZE_ACCELERATION_MSG_ID,
        PEDALS_MSG_ID,
        LWS_STANDARD_MSG_ID,
        BMS_VOLTAGES_MSG_ID,
        PACK_ANALOG_MSG_ID,
        MOTOR_SPEEDS_MSG_ID,
        MOTOR_TEMPS_MSG_ID,
        IGBT_TEMPS_MSG_ID,
        VCU_DRIVER_REQUEST_MSG_ID,
        BL_TORQUE_VECTOR_START_MSG_ID,
        A_BOX_FAULT_EVENT_MSG_ID,
        DASHBOARD_FAULT_EVENT_MSG_ID,
        MAIN_MODULE_FAULT_EVENT_MSG_ID,
        PDU_FAULT_EVENT_MSG_ID,
        A_BOX_FAULT_SYNC_MSG_ID,
        DASHBOARD_FAULT_SYNC_MSG_ID,
        MAIN_MODULE_FAULT_SYNC_MSG_ID,
        PDU_FAULT_SYNC_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN2,
        sid_list, 19,
        NULL, 0
    );
}

#endif // TORQUE_VECTOR_H
