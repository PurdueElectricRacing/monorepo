/**
 * @file main.c
 * @brief "Dashboard" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Chris Mcgalliard (cpmcgalliard@gmail.com)
 */

#include "main.h"

/* System Includes */
#include "can_library/faults_common.h"
#include "can_library/generated/DASHBOARD.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usart/usart.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "driver_interface.h"
#include "lap_timer.h"
#include "lcd.h"
#include "pedals.h"
#include "telemetry.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    // On-board LEDs
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // External LEDs
    PHAL_GPIO_INIT_OUTPUT(PRCHG_LED_PORT, PRCHG_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(IMD_LED_PORT, IMD_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(BMS_LED_PORT, BMS_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(REGEN_LED_PORT, REGEN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Main Button inputs
    PHAL_GPIO_INIT_INPUT(SELECT_BUTTON_PORT, SELECT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(DOWN_BUTTON_PORT, DOWN_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(UP_BUTTON_PORT, UP_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(LEFT_BUTTON_PORT, LEFT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(RIGHT_BUTTON_PORT, RIGHT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(START_BUTTON_PORT, START_BUTTON_PIN, GPIO_INPUT_PULL_UP),

    // Steering Wheel buttons
    PHAL_GPIO_INIT_INPUT(REGEN_TOGGLE_PORT, REGEN_TOGGLE_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(MARK_DATA_PORT, MARK_DATA_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(EBB_MINUS_PORT, EBB_MINUS_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(EBB_PLUS_PORT, EBB_PLUS_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(LAP_SET_PORT, LAP_SET_PIN, GPIO_INPUT_PULL_UP),
    PHAL_GPIO_INIT_INPUT(TV1_MINUS_PORT, TV1_MINUS_PIN, GPIO_INPUT_PULL_UP),

    // VCAN
    PHAL_PIN_DEFS_FDCAN2_RX_PB5,
    PHAL_PIN_DEFS_FDCAN2_TX_PB6,
    // SCAN
    PHAL_PIN_DEFS_FDCAN3_RX_PA8,
    PHAL_PIN_DEFS_FDCAN3_TX_PB4,

    // LCD
    PHAL_PIN_DEFS_USART1_TX_PA9,
    PHAL_PIN_DEFS_USART1_RX_PA10,
};

/* ADC Configuration */
static const PHAL_ADC_InitConfig_t adc_config[] = {
    PHAL_ADC_INIT_ON(THROTTLE1_PORT, THROTTLE1_PIN, ADC1),
    PHAL_ADC_INIT_ON(THROTTLE2_PORT, THROTTLE2_PIN, ADC1),
    PHAL_ADC_INIT_ON(REGEN1_PORT, REGEN1_PIN, ADC1),
    PHAL_ADC_INIT_ON(REGEN2_PORT, REGEN2_PIN, ADC1),
    PHAL_ADC_INIT_ON(BRAKE1_PRESSURE_PORT, BRAKE1_PRESSURE_PIN, ADC1),
    PHAL_ADC_INIT_ON(BRAKE2_PRESSURE_PORT, BRAKE2_PRESSURE_PIN, ADC1),
};

/* Function Prototypes */
void sweep_external_leds();
void service_start_button();
extern void HardFault_Handler();
void calibrate_LWS();

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(pedals_periodic, PEDALS_PERIOD_MS, TASK_PRIORITY_HIGH, STACK_1024);
RTOS_DEFINE_TASK(fault_library_periodic, DASHBOARD_FAULT_SYNC_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_1024);
RTOS_DEFINE_TASK(driver_interface_periodic, DRIVER_INTERFACE_PERIOD_MS, TASK_PRIORITY_LOW, STACK_1024);
RTOS_DEFINE_TASK(report_telemetry_25hz, TELEMETRY_25HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_02hz, TELEMETRY_02HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(lap_timer_periodic, GPS_COORDINATES_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
// RTOS_DEFINE_TASK(calibrate_LWS, 0, TASK_PRIORITY_LOW, STACK_512); // ! only enable for calibration
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(sweep_external_leds);

int main(void) {
    // Hardware Initialization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (false == PHAL_USART_init(LCD_USART, LCD_BAUD_RATE, PHAL_RCC_getAPB2ClockHz())) {
        HardFault_Handler();
    }
    if (!PHAL_ADC_init(adc_config, countof(adc_config))) {
        HardFault_Handler();
    }

    CAN_init();

    // Software Initialization
    START_CAN_TASKS();
    CAN_SEND_dash_init(WDG_get_CSR());
    RTOS_START_TASK(pedals_periodic);
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(driver_interface_periodic);
    RTOS_START_TASK(report_telemetry_25hz);
    RTOS_START_TASK(report_telemetry_02hz);
    RTOS_START_TASK(lap_timer_periodic);
    // RTOS_START_TASK(calibrate_LWS); // ! only enable for calibration
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

// jose was here

void sweep_external_leds() {
    static uint32_t sweep_index = 0;

    switch (sweep_index++ % 4) {
        case 0:
            PHAL_GPIO_write(PRCHG_LED_PORT, PRCHG_LED_PIN, 1);
            PHAL_GPIO_write(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_GPIO_write(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 1:
            PHAL_GPIO_write(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_GPIO_write(IMD_LED_PORT, IMD_LED_PIN, 1);
            PHAL_GPIO_write(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 2:
            PHAL_GPIO_write(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_GPIO_write(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_GPIO_write(BMS_LED_PORT, BMS_LED_PIN, 1);
            PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 3:
            PHAL_GPIO_write(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_GPIO_write(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_GPIO_write(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, 1);
            break;
    }
}

// ! LWS calibration proceedure: send RESET CCW, then ZERO CCW
void calibrate_LWS() {
    // CCW = command code word
    static constexpr uint8_t CONFIG_CCW_RESET = 0x5;
    CAN_SEND_LWS_Config(CONFIG_CCW_RESET);
    
    RTOS_delay_ms(200);

    // CCW = command code word
    static constexpr uint8_t CONFIG_CCW_ZERO = 0x3;
    CAN_SEND_LWS_Config(CONFIG_CCW_ZERO);

    // Delete task
    vTaskDelete(NULL);
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL = 0;
    ERROR_LED_PORT->BSRR = (1U << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
