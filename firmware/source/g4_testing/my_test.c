
#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_MY)

#include <string.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13
};

void HardFault_Handler();

static void send_stuff(void);

static uint32_t tx_count = 0;

RTOS_DEFINE_TASK(send_stuff, 400, TASK_PRIORITY_HIGH, STACK_256);


int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_170MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Send on CAN2, receive on CAN3
    PHAL_FDCAN_init(FDCAN2, FDCAN_BAUD_500K);
    
    uint32_t sids[8] = {0x300, 0x301};
    uint32_t xids[8] = {0x1ABCDE1, 0x1ABCDE2, 0x1ABCDE3};
    PHAL_FDCAN_setFilters(FDCAN2, sids, 2, xids, 3);

    // Create threads
    RTOS_START_TASK(send_stuff);


    // NVIC
    NVIC_SetPriority(FDCAN2_IT0_IRQn, 6);
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);

    vTaskStartScheduler();

    return 0;
}


// static void PHAL_FDCAN_testExtended(void) {
//     CanMsgTypeDef_t msg;
//     msg.Bus            = FDCAN2;
//     msg.IDE   = true;
//     msg.ExtId          = 0x1ABCDE0 + 1;
//     uint8_t payload[8] = {'E', 'X', 'T', 'I', 'D', '_', 'T', 'X'};
//     msg.DLC            = sizeof(payload);
//     memcpy(msg.Data, payload, sizeof(payload));
//     PHAL_FDCAN_send(&msg);
// }

static void PHAL_FDCAN_testStandard(void) {
    CanMsgTypeDef_t msg;
    msg.Bus            = FDCAN2;
    msg.IDE   = false;
    msg.StdId          = 0x300 + 4;
    uint8_t payload[8] = {'S', 'T', 'D', 'I', 'D', '_', 'T', 'X'};
    msg.DLC            = sizeof(payload);
    memcpy(msg.Data, payload, sizeof(payload));
    PHAL_FDCAN_send(&msg);
}

static void send_stuff(void) {
    PHAL_FDCAN_testStandard();
    // PHAL_FDCAN_testExtended();
    tx_count++;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_FDCAN
