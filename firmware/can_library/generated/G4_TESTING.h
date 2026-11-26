#ifndef G4_TESTING_H
#define G4_TESTING_H

/**
 * @file G4_TESTING.h
 * @brief Auto-generated CAN driver for G4_TESTING.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include <string.h>

#include "can_library/can_codec.h"
#include "can_library/can_common.h"
#include "can_library/generated/can_types.h"
#include "can_library/generated/can_version.h"
#include "can_library/generated/TEST_CAN.h"

// Node CAN configuration
#if !defined(STM32G474xx)
#error "CAN_NODE_G4_TESTING uses peripherals for STM32G474xx"
#endif

#define USE_FDCAN2

void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg);

// Scaling Macros
// canpiler_test
static constexpr float PACK_COEFF_CANPILER_TEST_CURRENT = 10.0f;
static constexpr float PACK_COEFF_CANPILER_TEST_VOLTAGE = 1000.0f;
static constexpr float PACK_COEFF_CANPILER_TEST_PRESSURE = 0.5f;

// Offset Macros
// canpiler_test
static constexpr float OFFSET_CANPILER_TEST_TEMPERATURE = -40.0f;
static constexpr float OFFSET_CANPILER_TEST_VOLTAGE = 2.5f;
static constexpr float OFFSET_CANPILER_TEST_PRESSURE = 20.0f;

// Data Structures
typedef struct {
} can_data_t;

extern volatile can_data_t can_data;

// Callback Declarations

// RX Dispatcher
static inline void CAN_rx_dispatcher(uint32_t id, const uint8_t *data, uint8_t len, const FDCAN_GlobalTypeDef *bus) {
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
static inline void CAN_SEND_IVT_S_command(
    uint8_t value_opcode,
    uint8_t value_parameter_1,
    uint8_t value_parameter_2,
    uint8_t value_parameter_3,
    uint8_t value_parameter_4,
    uint8_t value_parameter_5,
    uint8_t value_parameter_6,
    uint8_t value_parameter_7
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = IVT_S_COMMAND_MSG_ID,
        .IDE = false,
        .DLC = IVT_S_COMMAND_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_opcode = (uint64_t)value_opcode;
    payload = CAN_pack_raw_signal(payload, sig_raw_opcode, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_parameter_1 = (uint64_t)value_parameter_1;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_1, 0xFF, 8, BSWAP_NONE);
    uint64_t sig_raw_parameter_2 = (uint64_t)value_parameter_2;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_2, 0xFF, 16, BSWAP_NONE);
    uint64_t sig_raw_parameter_3 = (uint64_t)value_parameter_3;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_3, 0xFF, 24, BSWAP_NONE);
    uint64_t sig_raw_parameter_4 = (uint64_t)value_parameter_4;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_4, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_parameter_5 = (uint64_t)value_parameter_5;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_5, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_parameter_6 = (uint64_t)value_parameter_6;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_6, 0xFF, 48, BSWAP_NONE);
    uint64_t sig_raw_parameter_7 = (uint64_t)value_parameter_7;
    payload = CAN_pack_raw_signal(payload, sig_raw_parameter_7, 0xFF, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IVT_S_COMMAND_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_IZZE_IMU_config(
    uint16_t value_programming_constant,
    uint16_t value_NEW_CAN_BASE_ID,
    izze_gyro_scale_t value_gyro_scale,
    izze_accel_scale_t value_accel_scale,
    izze_output_data_rate_t value_output_data_rate,
    izze_bit_rate_t value_bit_rate
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = IZZE_IMU_CONFIG_MSG_ID,
        .IDE = false,
        .DLC = IZZE_IMU_CONFIG_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_programming_constant = (uint64_t)value_programming_constant;
    payload = CAN_pack_raw_signal(payload, sig_raw_programming_constant, 0xFFFF, 0, BSWAP_16);
    uint64_t sig_raw_NEW_CAN_BASE_ID = (uint64_t)value_NEW_CAN_BASE_ID;
    payload = CAN_pack_raw_signal(payload, sig_raw_NEW_CAN_BASE_ID, 0xFFFF, 16, BSWAP_16);
    uint64_t sig_raw_gyro_scale = (uint64_t)value_gyro_scale;
    payload = CAN_pack_raw_signal(payload, sig_raw_gyro_scale, 0xFF, 32, BSWAP_NONE);
    uint64_t sig_raw_accel_scale = (uint64_t)value_accel_scale;
    payload = CAN_pack_raw_signal(payload, sig_raw_accel_scale, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_output_data_rate = (uint64_t)value_output_data_rate;
    payload = CAN_pack_raw_signal(payload, sig_raw_output_data_rate, 0xFF, 48, BSWAP_NONE);
    uint64_t sig_raw_bit_rate = (uint64_t)value_bit_rate;
    payload = CAN_pack_raw_signal(payload, sig_raw_bit_rate, 0xFF, 56, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, IZZE_IMU_CONFIG_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}

[[gnu::always_inline]]
static inline void CAN_SEND_canpiler_test(
    uint8_t value_temperature,
    int16_t value_current,
    uint16_t value_voltage,
    uint8_t value_pressure,
    uint8_t value_status
) {
    CanMsgTypeDef_t outgoing = {
        .Bus = FDCAN2,
        .StdId = CANPILER_TEST_MSG_ID,
        .IDE = false,
        .DLC = CANPILER_TEST_DLC,
        .Data = {0}
    };

    uint64_t payload = 0;
    uint64_t sig_raw_temperature = (uint64_t)value_temperature;
    payload = CAN_pack_raw_signal(payload, sig_raw_temperature, 0xFF, 0, BSWAP_NONE);
    uint64_t sig_raw_current = (uint64_t)value_current;
    payload = CAN_pack_raw_signal(payload, sig_raw_current, 0xFFFF, 8, BSWAP_NONE);
    uint64_t sig_raw_voltage = (uint64_t)value_voltage;
    payload = CAN_pack_raw_signal(payload, sig_raw_voltage, 0xFFFF, 24, BSWAP_NONE);
    uint64_t sig_raw_pressure = (uint64_t)value_pressure;
    payload = CAN_pack_raw_signal(payload, sig_raw_pressure, 0xFF, 40, BSWAP_NONE);
    uint64_t sig_raw_status = (uint64_t)value_status;
    payload = CAN_pack_raw_signal(payload, sig_raw_status, 0xFF, 48, BSWAP_NONE);

    CAN_store_payload_u64(outgoing.Data, payload, CANPILER_TEST_DLC);

    CAN_enqueue_tx_FDCAN2(&outgoing);
}


// Peripheral initialization (board clocks and GPIO must be configured first)
static inline void CAN_init_peripherals(void) {
    PHAL_FDCAN_init(FDCAN2, TEST_CAN_BAUD_RATE);
}

// Filter configuration
// CAN filter setup for FDCAN2
static inline void FDCAN2_init_filters() {
    // No filters configured
    (void)0;
}

#endif // G4_TESTING_H
