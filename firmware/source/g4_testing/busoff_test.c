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
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12,
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
};

static void send_periodic(void);
static void send_tx_error_count(void);
static void induce_fdcan1_error(void);
static void busoff_recovery_task(void);

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(send_periodic, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(send_tx_error_count, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(induce_fdcan1_error, 400, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(busoff_recovery_task, 100, TASK_PRIORITY_NORMAL, 512);

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // FDCAN1 is used to induce bus-off
    // CAN_init() configures FDCAN2 (TEST_CAN) for telemetry
    PHAL_FDCAN_init(FDCAN1, FDCAN_BAUD_250K);

    CAN_init();

    START_CAN_TASKS();
    RTOS_START_TASK(send_periodic);
    RTOS_START_TASK(send_tx_error_count);
    RTOS_START_TASK(induce_fdcan1_error);
    RTOS_START_TASK(busoff_recovery_task);

    vTaskStartScheduler();

    return 0;
}

// Random data to verify transmission
static uint8_t temperature = 0;
static void send_periodic(void) {
    temperature++;
    int16_t current  = (int16_t)(-125.5f * PACK_COEFF_CANPILER_TEST_CURRENT);
    uint16_t voltage = (uint16_t)((3.700f - OFFSET_CANPILER_TEST_VOLTAGE) * PACK_COEFF_CANPILER_TEST_VOLTAGE);
    uint8_t pressure = (uint8_t)((300.0f - OFFSET_CANPILER_TEST_PRESSURE) * PACK_COEFF_CANPILER_TEST_PRESSURE);
    uint8_t status   = 165;

    CAN_SEND_canpiler_test(temperature, current, voltage, pressure, status);
}

// Transmit FDCAN1 TX error counter over TEST_CAN (FDCAN2)
static void send_tx_error_count(void) {
    uint8_t tx_counter = PHAL_FDCAN_txErrorCounter(FDCAN1);
    CAN_SEND_tx_errctr(tx_counter);
}

// Send frames on FDCAN1 to induce TX errors / bus-off
static void induce_fdcan1_error(void) {
    CanMsgTypeDef_t err_msg = {
        .Bus   = FDCAN1,
        .IDE   = false,
        .StdId = 0x123,
        .DLC   = 1,
        .Data  = {0xAA}
    };
    PHAL_FDCAN_send(&err_msg);
}

// Monitor active FDCAN peripherals and initiate bus-off recovery when needed
static void busoff_recovery_task(void) {
    if (PHAL_FDCAN_isBusOff(FDCAN1)) {
        PHAL_FDCAN_recoverBusOff(FDCAN1);
    }
    if (PHAL_FDCAN_isBusOff(FDCAN2)) {
        PHAL_FDCAN_recoverBusOff(FDCAN2);
    }
}

#endif // G4_TESTING_CHOSEN == TEST_BUSOFF
