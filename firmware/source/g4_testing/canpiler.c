#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_CANPILER)

#include <string.h>

#include "can_library/generated/A_BOX.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "main.h"
#include "can_library/faults_common.h"
#include "common/utils/countof.h"

GPIOInitConfig_t gpio_config[] = {
    // GPIO_INIT_FDCAN2RX_PB12,
    // GPIO_INIT_FDCAN2TX_PB13
    GPIO_INIT_FDCAN1RX_PA11,
    GPIO_INIT_FDCAN1TX_PA12,

};

void HardFault_Handler();

// void send_periodic() {
//     CAN_SEND_ccan_test(0x3);
// }

void send_periodic() {
    CAN_SEND_abox_version(GIT_HASH);
}

FREERTOS_DEFINE_TASK(CAN_rx_update, 0, TASK_PRIORITY_HIGH, STACK_2048);
FREERTOS_DEFINE_TASK(CAN_tx_update, 2, TASK_PRIORITY_NORMAL, STACK_2048);
FREERTOS_DEFINE_TASK(send_periodic, 10, TASK_PRIORITY_NORMAL, 1024);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);

    CAN_library_init();

    // NVIC
    NVIC_SetPriority(FDCAN1_IT0_IRQn, 6);
    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    FREERTOS_START_TASK(CAN_rx_update);
    FREERTOS_START_TASK(CAN_tx_update);
    FREERTOS_START_TASK(send_periodic);

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_CANPILER
