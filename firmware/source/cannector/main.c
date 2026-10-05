/**
 * @file main.c
 * @brief "CANnector" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "main.h"

#include "can_library/generated/MCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usb/usb.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "common/utils/countof.h"
#include "usb_device.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    // VCAN
    PHAL_PIN_DEFS_FDCAN1_RX_PB8,
    PHAL_PIN_DEFS_FDCAN1_TX_PB9,
    // MCAN
    PHAL_PIN_DEFS_FDCAN2_RX_PB5,
    PHAL_PIN_DEFS_FDCAN2_TX_PB6,
    // SCAN
    PHAL_PIN_DEFS_FDCAN3_RX_PA8,
    PHAL_PIN_DEFS_FDCAN3_TX_PA15,

    // LEDs
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(VCAN_LED_PORT, VCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MCAN_LED_PORT, MCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(SCAN_LED_PORT, SCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(USB_LED_PORT, USB_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Charger Mode detection
    PHAL_GPIO_INIT_INPUT(CHARGER_PORT, CHARGER_PIN, GPIO_INPUT_OPEN_DRAIN),
};

RTOS_DEFINE_TASK(usb_tx_periodic, 1, TASK_PRIORITY_HIGH, STACK_1024);
DEFINE_HEARTBEAT_TASK(nullptr);

RTOS_DEFINE_QUEUE(can_queue, timestamped_frame_t, 256);

void main() {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    PHAL_GPIO_init(gpio_config, countof(gpio_config));
    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, MCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, SCAN_BAUD_RATE);
    PHAL_USB_init();

    RTOS_INIT_QUEUE(can_queue);

    NVIC_SetPriority(FDCAN1_IT0_IRQn, 5);
    NVIC_SetPriority(FDCAN2_IT0_IRQn, 5);
    NVIC_SetPriority(FDCAN3_IT0_IRQn, 5);
    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN3_IT0_IRQn);

    START_HEARTBEAT_TASK();
    RTOS_START_TASK(usb_tx_periodic);

    vTaskStartScheduler();
}
