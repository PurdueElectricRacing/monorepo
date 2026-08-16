/**
 * @file node_defs.h
 * @brief Compile-time transport configuration for each G4 bootloader image.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * Each resident image listens on exactly one logical CAN bus. The selected
 * transport remains data-driven so targets may use different buses without
 * allowing one target to accept updates from multiple buses.
 */

#ifndef PER_BOOTLOADER_NODE_DEFS_H
#define PER_BOOTLOADER_NODE_DEFS_H

#include <stdbool.h>
#include <stdint.h>

#include "can_library/generated/VCAN.h"
#include "can_library/generated/can_version.h"
#include "common/bootloader/bootloader_common.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"

typedef enum {
    BL_BUS_VCAN,
    BL_BUS_MCAN,
    BL_BUS_CCAN,
    BL_BUS_SCAN,
    BL_BUS_GCAN,
} BLBus_t;

/**
 * @brief All board-specific details needed by the bootloader transport.
 *
 * IDs are kept as raw CAN IDs; is_extended_id determines whether they are
 * placed in the standard or extended field of a FDCAN frame. GPIO entries
 * are values rather than pointers so every entry can be initialized in the
 * same table without transport-specific code in bootloader.c.
 */
typedef struct {
    BLBus_t bus;
    FDCAN_GlobalTypeDef *peripheral;
    PHAL_FDCAN_BaudRate_t baud_rate;
    GPIOInitConfig_t rx_gpio;
    GPIOInitConfig_t tx_gpio;
    bool is_extended_id;
    uint32_t command_id;
    uint32_t data_id;
    uint32_t response_id;
    uint8_t response_dlc;
    uint32_t info_id;
    uint8_t info_dlc;
    uint8_t target_id;
} BLTransportConfig_t;

#define APP_MAIN_MODULE 1
#define APP_DASHBOARD 2
#define APP_A_BOX 3
#define APP_TORQUE_VECTOR 4
#define APP_FRONT_DRIVELINE 5
#define APP_REAR_DRIVELINE 6

#if (APP_ID == APP_MAIN_MODULE)
#define BL_TARGET_ID BOOTLOADER_TARGET_MAIN_MODULE
/* VCAN: FDCAN2 on PB12/PB13. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN2, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN2RX_PB12, \
        .tx_gpio = GPIO_INIT_FDCAN2TX_PB13, \
        .is_extended_id = false, \
        .command_id = BL_MAIN_MODULE_CMD_MSG_ID, \
        .data_id = BL_MAIN_MODULE_DATA_MSG_ID, \
        .response_id = BL_MAIN_MODULE_RESP_MSG_ID, \
        .response_dlc = BL_MAIN_MODULE_RESP_DLC, \
        .info_id = BL_MAIN_MODULE_INFO_MSG_ID, \
        .info_dlc = BL_MAIN_MODULE_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#elif (APP_ID == APP_DASHBOARD)
#define BL_TARGET_ID BOOTLOADER_TARGET_DASHBOARD
/* VCAN: FDCAN2 on PB5/PB6. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN2, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN2RX_PB5, \
        .tx_gpio = GPIO_INIT_FDCAN2TX_PB6, \
        .is_extended_id = false, \
        .command_id = BL_DASHBOARD_CMD_MSG_ID, \
        .data_id = BL_DASHBOARD_DATA_MSG_ID, \
        .response_id = BL_DASHBOARD_RESP_MSG_ID, \
        .response_dlc = BL_DASHBOARD_RESP_DLC, \
        .info_id = BL_DASHBOARD_INFO_MSG_ID, \
        .info_dlc = BL_DASHBOARD_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#elif (APP_ID == APP_A_BOX)
#define BL_TARGET_ID BOOTLOADER_TARGET_A_BOX
/* VCAN: FDCAN1 on PA11/PA12. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN1, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN1RX_PA11, \
        .tx_gpio = GPIO_INIT_FDCAN1TX_PA12, \
        .is_extended_id = false, \
        .command_id = BL_A_BOX_CMD_MSG_ID, \
        .data_id = BL_A_BOX_DATA_MSG_ID, \
        .response_id = BL_A_BOX_RESP_MSG_ID, \
        .response_dlc = BL_A_BOX_RESP_DLC, \
        .info_id = BL_A_BOX_INFO_MSG_ID, \
        .info_dlc = BL_A_BOX_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#elif (APP_ID == APP_TORQUE_VECTOR)
#define BL_TARGET_ID BOOTLOADER_TARGET_TORQUE_VECTOR
/* VCAN: FDCAN2 on PB12/PB13. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN2, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN2RX_PB12, \
        .tx_gpio = GPIO_INIT_FDCAN2TX_PB13, \
        .is_extended_id = false, \
        .command_id = BL_TORQUE_VECTOR_CMD_MSG_ID, \
        .data_id = BL_TORQUE_VECTOR_DATA_MSG_ID, \
        .response_id = BL_TORQUE_VECTOR_RESP_MSG_ID, \
        .response_dlc = BL_TORQUE_VECTOR_RESP_DLC, \
        .info_id = BL_TORQUE_VECTOR_INFO_MSG_ID, \
        .info_dlc = BL_TORQUE_VECTOR_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#elif (APP_ID == APP_FRONT_DRIVELINE)
#define BL_TARGET_ID BOOTLOADER_TARGET_FRONT_DRIVELINE
/* VCAN: FDCAN2 on PB5/PB6. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN2, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN2RX_PB5, \
        .tx_gpio = GPIO_INIT_FDCAN2TX_PB6, \
        .is_extended_id = false, \
        .command_id = BL_FRONT_DRIVELINE_CMD_MSG_ID, \
        .data_id = BL_FRONT_DRIVELINE_DATA_MSG_ID, \
        .response_id = BL_FRONT_DRIVELINE_RESP_MSG_ID, \
        .response_dlc = BL_FRONT_DRIVELINE_RESP_DLC, \
        .info_id = BL_FRONT_DRIVELINE_INFO_MSG_ID, \
        .info_dlc = BL_FRONT_DRIVELINE_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#elif (APP_ID == APP_REAR_DRIVELINE)
#define BL_TARGET_ID BOOTLOADER_TARGET_REAR_DRIVELINE
/* VCAN: FDCAN2 on PB5/PB6. */
#define BL_TRANSPORT_CONFIG \
    { \
        .bus = BL_BUS_VCAN, \
        .peripheral = FDCAN2, \
        .baud_rate = FDCAN_BAUD_500K, \
        .rx_gpio = GPIO_INIT_FDCAN2RX_PB5, \
        .tx_gpio = GPIO_INIT_FDCAN2TX_PB6, \
        .is_extended_id = false, \
        .command_id = BL_REAR_DRIVELINE_CMD_MSG_ID, \
        .data_id = BL_REAR_DRIVELINE_DATA_MSG_ID, \
        .response_id = BL_REAR_DRIVELINE_RESP_MSG_ID, \
        .response_dlc = BL_REAR_DRIVELINE_RESP_DLC, \
        .info_id = BL_REAR_DRIVELINE_INFO_MSG_ID, \
        .info_dlc = BL_REAR_DRIVELINE_INFO_DLC, \
        .target_id = BL_TARGET_ID, \
    }
#else
#error "APP_ID is missing or is not a supported G4 bootloader node"
#endif

static const BLTransportConfig_t bl_transport = BL_TRANSPORT_CONFIG;

#endif /* PER_BOOTLOADER_NODE_DEFS_H */
