/**
 * @file main.c
 * @brief "Main Module" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/faults_common.h"
#include "can_library/generated/MAIN_MODULE.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "sdc.h"
#include "telemetry.h"
#include "powertrain.h"
#include "vehicle_fsm.h"

/* PER HAL Initialization Structures */
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Status LEDs
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // TSAL
    PHAL_GPIO_INIT_OUTPUT(TSAL_RED_CTRL_PORT, TSAL_RED_CTRL_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(TSAL_GREEN_CTRL_PORT, TSAL_GREEN_CTRL_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(TSAL_RTM_ENABLE_PORT, TSAL_RTM_ENABLE_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_INPUT(TSAL_FAULT_PORT, TSAL_FAULT_PIN, GPIO_INPUT_OPEN_DRAIN),

    // Brake and Buzzer
    PHAL_GPIO_INIT_OUTPUT(BRAKE_LIGHT_PORT, BRAKE_LIGHT_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(BUZZER_PORT, BUZZER_PIN, GPIO_OUTPUT_LOW_SPEED),

    // SDC
    PHAL_GPIO_INIT_OUTPUT(ECU_SDC_CTRL_PORT, ECU_SDC_CTRL_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_INPUT(SDC_MUX_PORT, SDC_MUX_PIN, GPIO_INPUT_PULL_DOWN), // pull down (SDC open) for floating case
    PHAL_GPIO_INIT_OUTPUT(SDC_MUX_S3_PORT, SDC_MUX_S3_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(SDC_MUX_S2_PORT, SDC_MUX_S2_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(SDC_MUX_S1_PORT, SDC_MUX_S1_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(SDC_MUX_S0_PORT, SDC_MUX_S0_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Input status pins
    PHAL_GPIO_INIT_INPUT(BMS_STATUS_PORT, BMS_STATUS_PIN, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_INPUT(VBATT_ECU_PORT, VBATT_ECU_PIN, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_INPUT(VMC_ECU_PORT, VMC_ECU_PIN, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_INPUT(NOT_PRECHARGE_COMPLETE_PORT, NOT_PRECHARGE_COMPLETE_PIN, GPIO_INPUT_PULL_DOWN),

    // VCAN
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,

    // MCAN
    PHAL_PIN_DEFS_FDCAN3_TX_PB3,
    PHAL_PIN_DEFS_FDCAN3_RX_PA15
};

extern void HardFault_Handler(void);

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(vehicle_fsm_periodic, VEHICLE_FSM_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_2048);
RTOS_DEFINE_TASK(fault_library_periodic, MAIN_MODULE_FAULT_SYNC_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_1024);
RTOS_DEFINE_TASK(SDC_task_periodic, SDC_TASK_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_50hz, TELEMETRY_50HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_1hz, TELEMETRY_1HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_02hz, TELEMETRY_02HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main(void) {
    // Hardware Initialization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // CAN init
    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, MCAN_BAUD_RATE);
    CAN_init();

    powertrain_init();

    PHAL_GPIO_write(ECU_SDC_CTRL_PORT, ECU_SDC_CTRL_PIN, true); // set SDC high

    START_CAN_TASKS();
    CAN_SEND_main_init(WDG_get_CSR());
    RTOS_START_TASK(vehicle_fsm_periodic);
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(SDC_task_periodic);
    RTOS_START_TASK(report_telemetry_50hz);
    RTOS_START_TASK(report_telemetry_1hz);
    RTOS_START_TASK(report_telemetry_02hz);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL = 0;
    ERROR_LED_PORT->BSRR = (1U << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
