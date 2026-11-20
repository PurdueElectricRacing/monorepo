#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_BUSOFF)

#include <stdint.h>

#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "can_library/can_common.h"
#include "can_library/generated/G4_TESTING.h"


PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
    PHAL_PIN_DEFS_FDCAN3_RX_PA8,
    PHAL_PIN_DEFS_FDCAN3_TX_PB4,
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12
};

void HardFault_Handler();

// random data to see if it transfers
uint8_t temperature = 0;
void send_periodic() {
    temperature++;
    int16_t current = (int16_t)(-125.5f * PACK_COEFF_CANPILER_TEST_CURRENT);
    uint16_t voltage = (uint16_t)((3.700f - OFFSET_CANPILER_TEST_VOLTAGE) * PACK_COEFF_CANPILER_TEST_VOLTAGE);
    uint8_t pressure = (uint8_t)((300.0f - OFFSET_CANPILER_TEST_PRESSURE) * PACK_COEFF_CANPILER_TEST_PRESSURE);
    uint8_t status = 165;

    CAN_SEND_canpiler_test(temperature, current, voltage, pressure, status);
}

// sending tx error count
void send_tx_error_count() {
    uint32_t txCounter = PHAL_FDCAN_txErrCounter(FDCAN2);
    CAN_SEND_tx_errctr(txCounter);
}

// increase fdcan2 error counter
void induce_fdcan2_error() {
    CanMsgTypeDef_t err_msg = {
        .Bus   = FDCAN2,
        .IDE   = false,
        .StdId = 0x123,
        .DLC   = 1,
        .Data  = { 0xAA }
    };
    PHAL_FDCAN_send(&err_msg);
}

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(send_periodic, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(send_tx_error_count, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(induce_fdcan2_error, 400, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(PHAL_FDCAN_busoff_recovery_task, 100, TASK_PRIORITY_NORMAL, 512);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    
    PHAL_FDCAN_init(FDCAN2, FDCAN_BAUD_250K);

    CAN_init();

    START_CAN_TASKS();  
    RTOS_START_TASK(send_periodic);
    RTOS_START_TASK(send_tx_error_count);
    RTOS_START_TASK(induce_fdcan2_error);
    RTOS_START_TASK(PHAL_FDCAN_busoff_recovery_task);

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_BUSOFF
