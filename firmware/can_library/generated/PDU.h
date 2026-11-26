#ifndef PDU_H
#define PDU_H

/**
 * @file PDU.h
 * @brief Auto-generated CAN driver for PDU.
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
#error "CAN_NODE_PDU uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN1
#define USE_FDCAN2

void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg);
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);

// Scaling Macros
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

// Offset Macros

// Data Structures
typedef struct {
    main_hb_data_t main_hb;
    motor_temps_data_t motor_temps;
    igbt_temps_data_t igbt_temps;
    bms_temps_data_t bms_temps;
    a_box_fault_event_data_t a_box_fault_event;
    dashboard_fault_event_data_t dashboard_fault_event;
    main_module_fault_event_data_t main_module_fault_event;
    torque_vector_fault_event_data_t torque_vector_fault_event;
    a_box_fault_sync_data_t a_box_fault_sync;
    dashboard_fault_sync_data_t dashboard_fault_sync;
    main_module_fault_sync_data_t main_module_fault_sync;
    torque_vector_fault_sync_data_t torque_vector_fault_sync;
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations
extern void a_box_fault_event_CALLBACK();
extern void dashboard_fault_event_CALLBACK();
extern void main_module_fault_event_CALLBACK();
extern void torque_vector_fault_event_CALLBACK();
extern void a_box_fault_sync_CALLBACK();
extern void dashboard_fault_sync_CALLBACK();
extern void main_module_fault_sync_CALLBACK();
extern void torque_vector_fault_sync_CALLBACK();

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
    if (bus == FDCAN1) {
        switch (id) {
            case MAIN_HB_MSG_ID: {
                uint64_t host_data = CAN_load_payload_u64(data, len);

                uint64_t sig_raw_car_state = CAN_unpack_raw_signal(host_data, 0xFF, 0, BSWAP_NONE);
                can_data.main_hb.car_state = (car_state_t)sig_raw_car_state;

                can_data.main_hb.last_rx = xTaskGetTickCount();
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
}

// Stale Checking
static bool is_main_hb_stale() {
    return (xTaskGetTickCount() - can_data.main_hb.last_rx) > MAIN_HB_STALE_TIMEOUT_MS;
}
static bool is_motor_temps_stale() {
    return (xTaskGetTickCount() - can_data.motor_temps.last_rx) > MOTOR_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_igbt_temps_stale() {
    return (xTaskGetTickCount() - can_data.igbt_temps.last_rx) > IGBT_TEMPS_STALE_TIMEOUT_MS;
}
static bool is_bms_temps_stale() {
    return (xTaskGetTickCount() - can_data.bms_temps.last_rx) > BMS_TEMPS_STALE_TIMEOUT_MS;
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
static bool is_torque_vector_fault_sync_stale() {
    return (xTaskGetTickCount() - can_data.torque_vector_fault_sync.last_rx) > TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS;
}

// Data initialization
static inline void CAN_init_data(void) {
    memset((void *)&can_data, 0, sizeof(can_data));
    // Underflow the last_rx timers to ensure messages are stale until first rx
    // Setup the stale function pointers
    can_data.main_hb.last_rx = (uint32_t) -(MAIN_HB_STALE_TIMEOUT_MS + 1);
    can_data.main_hb.is_stale = is_main_hb_stale;
    can_data.motor_temps.last_rx = (uint32_t) -(MOTOR_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.motor_temps.is_stale = is_motor_temps_stale;
    can_data.igbt_temps.last_rx = (uint32_t) -(IGBT_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.igbt_temps.is_stale = is_igbt_temps_stale;
    can_data.bms_temps.last_rx = (uint32_t) -(BMS_TEMPS_STALE_TIMEOUT_MS + 1);
    can_data.bms_temps.is_stale = is_bms_temps_stale;
    can_data.a_box_fault_sync.last_rx = (uint32_t) -(A_BOX_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.a_box_fault_sync.is_stale = is_a_box_fault_sync_stale;
    can_data.dashboard_fault_sync.last_rx = (uint32_t) -(DASHBOARD_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.dashboard_fault_sync.is_stale = is_dashboard_fault_sync_stale;
    can_data.main_module_fault_sync.last_rx = (uint32_t) -(MAIN_MODULE_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.main_module_fault_sync.is_stale = is_main_module_fault_sync_stale;
    can_data.torque_vector_fault_sync.last_rx = (uint32_t) -(TORQUE_VECTOR_FAULT_SYNC_STALE_TIMEOUT_MS + 1);
    can_data.torque_vector_fault_sync.is_stale = is_torque_vector_fault_sync_stale;
}

// TX Functions
[[gnu::always_inline]]
static inline void CAN_SEND_cooling_outputs(
    bool value_is_powertrain_pumps_enabled,
    bool value_is_hx_fan_enabled,
    bool value_is_batt_fans_enabled
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = COOLING_OUTPUTS_MSG_ID,
        .IDE = false,
        .DLC = COOLING_OUTPUTS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_is_powertrain_pumps_enabled = (uint64_t)value_is_powertrain_pumps_enabled;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_powertrain_pumps_enabled, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_is_hx_fan_enabled = (uint64_t)value_is_hx_fan_enabled;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_hx_fan_enabled, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_is_batt_fans_enabled = (uint64_t)value_is_batt_fans_enabled;
    payload = CAN_pack_raw_signal(payload, sig_raw_is_batt_fans_enabled, 0x1, 2, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, COOLING_OUTPUTS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_switch_cs(
    switch_id_t value_switch_id,
    uint16_t value_current
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = SWITCH_CS_MSG_ID,
        .IDE = false,
        .DLC = SWITCH_CS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_switch_id = (uint64_t)value_switch_id;
    payload = CAN_pack_raw_signal(payload, sig_raw_switch_id, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_current = (uint64_t)value_current;
    payload = CAN_pack_raw_signal(payload, sig_raw_current, 0xFFFF, 8, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, SWITCH_CS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_can_overflow(
    uint32_t value_rx_overflow,
    uint32_t value_tx_overflow
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = PDU_CAN_OVERFLOW_MSG_ID,
        .IDE = false,
        .DLC = PDU_CAN_OVERFLOW_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_rx_overflow = (uint64_t)value_rx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_rx_overflow, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_overflow = (uint64_t)value_tx_overflow;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_overflow, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_CAN_OVERFLOW_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_can_tx_actions(
    uint32_t value_enqueue_count,
    uint32_t value_task_wake_count
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = PDU_CAN_TX_ACTIONS_MSG_ID,
        .IDE = false,
        .DLC = PDU_CAN_TX_ACTIONS_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_enqueue_count = (uint64_t)value_enqueue_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_enqueue_count, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_task_wake_count = (uint64_t)value_task_wake_count;
    payload = CAN_pack_raw_signal(payload, sig_raw_task_wake_count, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_CAN_TX_ACTIONS_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_can_tx_messages(
    uint32_t value_tx_sent,
    uint32_t value_tx_callbacks
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = PDU_CAN_TX_MESSAGES_MSG_ID,
        .IDE = false,
        .DLC = PDU_CAN_TX_MESSAGES_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_tx_sent = (uint64_t)value_tx_sent;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_sent, 0xFFFFFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_tx_callbacks = (uint64_t)value_tx_callbacks;
    payload = CAN_pack_raw_signal(payload, sig_raw_tx_callbacks, 0xFFFFFFFF, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_CAN_TX_MESSAGES_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_init(
    uint32_t value_csr
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = PDU_INIT_MSG_ID,
        .IDE = false,
        .DLC = PDU_INIT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_csr = (uint64_t)value_csr;
    payload = CAN_pack_raw_signal(payload, sig_raw_csr, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_INIT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_version(
    uint32_t value_git_hash
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = PDU_VERSION_MSG_ID,
        .IDE = false,
        .DLC = PDU_VERSION_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_git_hash = (uint64_t)value_git_hash;
    payload = CAN_pack_raw_signal(payload, sig_raw_git_hash, 0xFFFFFFFF, 0, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_VERSION_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_fault_event(
    fault_id_t value_idx,
    uint16_t value_val,
    bool value_state
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = PDU_FAULT_EVENT_MSG_ID,
        .IDE = false,
        .DLC = PDU_FAULT_EVENT_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_idx = (uint64_t)value_idx;
    payload = CAN_pack_raw_signal(payload, sig_raw_idx, 0xFFFF, 0, BSWAP_NONE);
    uint64_t sig_raw_val = (uint64_t)value_val;
    payload = CAN_pack_raw_signal(payload, sig_raw_val, 0xFFFF, 16, BSWAP_NONE);
    uint64_t sig_raw_state = (uint64_t)value_state;
    payload = CAN_pack_raw_signal(payload, sig_raw_state, 0x1, 32, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_FAULT_EVENT_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_pdu_fault_sync(
    bool value_SDC_SWITCH_FAULT,
    bool value_SDC_LOOP_SWITCH_FAULT,
    bool value_MAIN_SWITCH_FAULT,
    bool value_DASH_SWITCH_FAULT,
    bool value_DAQ_SWITCH_FAULT,
    bool value_ABOX_SWITCH_FAULT,
    bool value_HV_BOARD_SWITCH_FAULT,
    bool value_TV_SWITCH_FAULT,
    bool value_DL_FRONT_SWITCH_FAULT,
    bool value_DL_REAR_SWITCH_FAULT,
    bool value_AMK1_SWITCH_FAULT,
    bool value_AMK2_SWITCH_FAULT,
    bool value_PUMP1_SWITCH_FAULT,
    bool value_PUMP2_SWITCH_FAULT,
    bool value_HXFAN_SWITCH_FAULT,
    bool value_FAN1_SWITCH_FAULT,
    bool value_FAN2_SWITCH_FAULT,
    bool value_FAN3_SWITCH_FAULT,
    bool value_FAN4_SWITCH_FAULT,
    bool value_FAN5_SWITCH_FAULT,
    bool value_EMETER_SWITCH_FAULT
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN1,
        .StdId = PDU_FAULT_SYNC_MSG_ID,
        .IDE = false,
        .DLC = PDU_FAULT_SYNC_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_SDC_SWITCH_FAULT = (uint64_t)value_SDC_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC_SWITCH_FAULT, 0x1, 0, BSWAP_NONE);
    uint64_t sig_raw_SDC_LOOP_SWITCH_FAULT = (uint64_t)value_SDC_LOOP_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_SDC_LOOP_SWITCH_FAULT, 0x1, 1, BSWAP_NONE);
    uint64_t sig_raw_MAIN_SWITCH_FAULT = (uint64_t)value_MAIN_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_MAIN_SWITCH_FAULT, 0x1, 2, BSWAP_NONE);
    uint64_t sig_raw_DASH_SWITCH_FAULT = (uint64_t)value_DASH_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_DASH_SWITCH_FAULT, 0x1, 3, BSWAP_NONE);
    uint64_t sig_raw_DAQ_SWITCH_FAULT = (uint64_t)value_DAQ_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_DAQ_SWITCH_FAULT, 0x1, 4, BSWAP_NONE);
    uint64_t sig_raw_ABOX_SWITCH_FAULT = (uint64_t)value_ABOX_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_ABOX_SWITCH_FAULT, 0x1, 5, BSWAP_NONE);
    uint64_t sig_raw_HV_BOARD_SWITCH_FAULT = (uint64_t)value_HV_BOARD_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_HV_BOARD_SWITCH_FAULT, 0x1, 6, BSWAP_NONE);
    uint64_t sig_raw_TV_SWITCH_FAULT = (uint64_t)value_TV_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_TV_SWITCH_FAULT, 0x1, 7, BSWAP_NONE);
    uint64_t sig_raw_DL_FRONT_SWITCH_FAULT = (uint64_t)value_DL_FRONT_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_DL_FRONT_SWITCH_FAULT, 0x1, 8, BSWAP_NONE);
    uint64_t sig_raw_DL_REAR_SWITCH_FAULT = (uint64_t)value_DL_REAR_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_DL_REAR_SWITCH_FAULT, 0x1, 9, BSWAP_NONE);
    uint64_t sig_raw_AMK1_SWITCH_FAULT = (uint64_t)value_AMK1_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK1_SWITCH_FAULT, 0x1, 10, BSWAP_NONE);
    uint64_t sig_raw_AMK2_SWITCH_FAULT = (uint64_t)value_AMK2_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_AMK2_SWITCH_FAULT, 0x1, 11, BSWAP_NONE);
    uint64_t sig_raw_PUMP1_SWITCH_FAULT = (uint64_t)value_PUMP1_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_PUMP1_SWITCH_FAULT, 0x1, 12, BSWAP_NONE);
    uint64_t sig_raw_PUMP2_SWITCH_FAULT = (uint64_t)value_PUMP2_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_PUMP2_SWITCH_FAULT, 0x1, 13, BSWAP_NONE);
    uint64_t sig_raw_HXFAN_SWITCH_FAULT = (uint64_t)value_HXFAN_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_HXFAN_SWITCH_FAULT, 0x1, 14, BSWAP_NONE);
    uint64_t sig_raw_FAN1_SWITCH_FAULT = (uint64_t)value_FAN1_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_FAN1_SWITCH_FAULT, 0x1, 15, BSWAP_NONE);
    uint64_t sig_raw_FAN2_SWITCH_FAULT = (uint64_t)value_FAN2_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_FAN2_SWITCH_FAULT, 0x1, 16, BSWAP_NONE);
    uint64_t sig_raw_FAN3_SWITCH_FAULT = (uint64_t)value_FAN3_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_FAN3_SWITCH_FAULT, 0x1, 17, BSWAP_NONE);
    uint64_t sig_raw_FAN4_SWITCH_FAULT = (uint64_t)value_FAN4_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_FAN4_SWITCH_FAULT, 0x1, 18, BSWAP_NONE);
    uint64_t sig_raw_FAN5_SWITCH_FAULT = (uint64_t)value_FAN5_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_FAN5_SWITCH_FAULT, 0x1, 19, BSWAP_NONE);
    uint64_t sig_raw_EMETER_SWITCH_FAULT = (uint64_t)value_EMETER_SWITCH_FAULT;
    payload = CAN_pack_raw_signal(payload, sig_raw_EMETER_SWITCH_FAULT, 0x1, 20, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, PDU_FAULT_SYNC_DLC);

    CAN_enqueue_tx_FDCAN1(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, SCAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN1
static inline void FDCAN1_init_filters() {
    static const uint32_t sid_list[] = {
        MAIN_HB_MSG_ID,
        MOTOR_TEMPS_MSG_ID,
        IGBT_TEMPS_MSG_ID,
        BMS_TEMPS_MSG_ID,
        A_BOX_FAULT_EVENT_MSG_ID,
        DASHBOARD_FAULT_EVENT_MSG_ID,
        MAIN_MODULE_FAULT_EVENT_MSG_ID,
        TORQUE_VECTOR_FAULT_EVENT_MSG_ID,
        A_BOX_FAULT_SYNC_MSG_ID,
        DASHBOARD_FAULT_SYNC_MSG_ID,
        MAIN_MODULE_FAULT_SYNC_MSG_ID,
        TORQUE_VECTOR_FAULT_SYNC_MSG_ID
    };


    PHAL_FDCAN_setFilters(
        FDCAN1,
        sid_list, 12,
        NULL, 0
    );
}
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    // No filters configured
    (void)0;
}

#endif // PDU_H
