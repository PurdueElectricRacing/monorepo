#ifndef TEST_CAN_H
#define TEST_CAN_H

/**
 * @file TEST_CAN.h
 * @brief Auto-generated CAN bus definitions for TEST_CAN.
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

static constexpr uint32_t TEST_CAN_BUS_ID = 4;
static constexpr PHAL_FDCAN_BaudRate_t TEST_CAN_BAUD_RATE = FDCAN_BAUD_1M;

static constexpr uint32_t CANPILER_TEST_MSG_ID = 0x1;
static constexpr uint8_t  CANPILER_TEST_DLC = 7;
static constexpr uint64_t CANPILER_TEST_LAYOUT_HASH = 0x1BFD6F3E013FAA7C;
static constexpr uint32_t CANPILER_TEST_PERIOD_MS = 100;
static constexpr uint32_t CANPILER_TEST_STALE_TIMEOUT_MS = 250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t temperature;
    int16_t current;
    uint16_t voltage;
    uint8_t pressure;
    uint8_t status;
    // End signal defs
} canpiler_test_data_t;

static constexpr uint32_t IVT_S_COMMAND_MSG_ID = 0x411;
static constexpr uint8_t  IVT_S_COMMAND_DLC = 8;
static constexpr uint64_t IVT_S_COMMAND_LAYOUT_HASH = 0x11F92B511B00EF68;

typedef struct {
    uint32_t last_rx;
    // Begin signal defs
    uint8_t opcode;
    uint8_t parameter_1;
    uint8_t parameter_2;
    uint8_t parameter_3;
    uint8_t parameter_4;
    uint8_t parameter_5;
    uint8_t parameter_6;
    uint8_t parameter_7;
    // End signal defs
} IVT_S_command_data_t;

static constexpr uint32_t IZZE_IMU_CONFIG_MSG_ID = 0x4EC;
static constexpr uint8_t  IZZE_IMU_CONFIG_DLC = 8;
static constexpr uint64_t IZZE_IMU_CONFIG_LAYOUT_HASH = 0x60968AFAB77C3623;
static constexpr uint32_t IZZE_IMU_CONFIG_PERIOD_MS = 1000;
static constexpr uint32_t IZZE_IMU_CONFIG_STALE_TIMEOUT_MS = 2500;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint16_t programming_constant;
    uint16_t NEW_CAN_BASE_ID;
    izze_gyro_scale_t gyro_scale;
    izze_accel_scale_t accel_scale;
    izze_output_data_rate_t output_data_rate;
    izze_bit_rate_t bit_rate;
    // End signal defs
} IZZE_IMU_config_data_t;


#endif // TEST_CAN_H
