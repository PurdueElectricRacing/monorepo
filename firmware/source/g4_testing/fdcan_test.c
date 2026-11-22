
#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_FDCAN)

#include <string.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
    PHAL_PIN_DEFS_FDCAN3_RX_PA8,
    PHAL_PIN_DEFS_FDCAN3_TX_PB4,
};

void HardFault_Handler();

static void can_tx_100hz(void);
static void can_rx_1khz(void);

static uint32_t rx_count = 0;
static uint32_t tx_count = 0;

RTOS_DEFINE_TASK(can_tx_100hz, 10, TASK_PRIORITY_HIGH, STACK_256);
RTOS_DEFINE_TASK(can_rx_1khz, 1, TASK_PRIORITY_HIGH, STACK_256);

RTOS_DEFINE_QUEUE(q_can_rx, CanMsgTypeDef_t, 256);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_170MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Send on CAN2, receive on CAN3
    PHAL_FDCAN_init(FDCAN2, FDCAN_BAUD_500K);
    PHAL_FDCAN_init(FDCAN3, FDCAN_BAUD_500K);
    
    uint32_t sids[8] = {0x300, 0x301};
    uint32_t xids[8] = {0x1ABCDE1, 0x1ABCDE2, 0x1ABCDE3};
    PHAL_FDCAN_setFilters(FDCAN2, sids, 2, xids, 3);
    PHAL_FDCAN_setFilters(FDCAN3, sids, 2, xids, 3);

    // Create threads
    RTOS_START_TASK(can_tx_100hz);
    RTOS_START_TASK(can_rx_1khz);

    RTOS_INIT_QUEUE(q_can_rx);

    // NVIC
    NVIC_SetPriority(FDCAN2_IT0_IRQn, 6);
    NVIC_SetPriority(FDCAN3_IT0_IRQn, 7);
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN3_IT0_IRQn);

    vTaskStartScheduler();

    return 0;
}

void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    rx_count++;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        BaseType_t xHigherPriorityTaskWoken = 0;
        xQueueSendFromISR(q_can_rx, msg, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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

static void can_tx_100hz(void) {
    PHAL_FDCAN_testStandard();
    // PHAL_FDCAN_testExtended();
    tx_count++;
}

volatile CanMsgTypeDef_t rx_frame_0;

static void can_rx_1khz(void) {
    CanMsgTypeDef_t rx_frame;
    while (xQueueReceive(q_can_rx, &rx_frame, (TickType_t)0) == pdTRUE) {
        rx_frame_0 = rx_frame;
        ;
    }
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_FDCAN
