
#include "can_library/generated/MCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usb/usb.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "common/utils/countof.h"
#include "pindefs.h"
#include "common/heartbeat/heartbeat.h"


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
};

void usb_thread_periodic(void);

RTOS_DEFINE_TASK(usb_thread_periodic, 1, TASK_PRIORITY_HIGH, STACK_1024);
DEFINE_HEARTBEAT_TASK(nullptr);

RTOS_DEFINE_QUEUE(can_queue, timestamped_frame_t, 128);


void main() {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    PHAL_GPIO_init(gpio_config, countof(gpio_config));
    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, MCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, SCAN_BAUD_RATE);
    PHAL_USB_init();

    RTOS_START_TASK(usb_thread_periodic);

    vTaskStartScheduler();
}

void led_manager() {

}

void usb_thread_periodic() {
    timestamped_frame_t usb_packet[4];
    static_assert(sizeof(usb_packet) == 64, "usb_packet max size is 64");

    size_t count = uxQueueMessagesWaiting(can_queue);

    if (count > 4) {
        count = 4;
    }

    for (size_t i = 0; i < count; i++) {
        xQueueReceive(can_queue, &usb_packet[i], 0);
    }

    size_t length = count * sizeof(timestamped_frame_t);
    (void)PHAL_USB_write(PHAL_USB_DATA_ENDPOINT, usb_packet, (uint16_t)length);
}