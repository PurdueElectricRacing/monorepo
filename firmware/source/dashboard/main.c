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
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usart/usart.h"
#include "common/strbuf/strbuf.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "driver_interface.h"
#include "lcd.h"
#include "pedals.h"
#include "telemetry.h"

GPIOInitConfig_t gpio_config[] = {
    // On-board LEDs
    GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // External LEDs
    GPIO_INIT_OUTPUT(PRCHG_LED_PORT, PRCHG_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(IMD_LED_PORT, IMD_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(BMS_LED_PORT, BMS_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(REGEN_LED_PORT, REGEN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Main Button inputs
    GPIO_INIT_INPUT(SELECT_BUTTON_PORT, SELECT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(DOWN_BUTTON_PORT, DOWN_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(UP_BUTTON_PORT, UP_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(LEFT_BUTTON_PORT, LEFT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(RIGHT_BUTTON_PORT, RIGHT_BUTTON_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(START_BUTTON_PORT, START_BUTTON_PIN, GPIO_INPUT_PULL_UP),

    // Steering Wheel buttons
    GPIO_INIT_INPUT(REGEN_TOGGLE_PORT, REGEN_TOGGLE_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(MARK_DATA_PORT, MARK_DATA_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(EBB_MINUS_PORT, EBB_MINUS_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(EBB_PLUS_PORT, EBB_PLUS_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(TV1_PLUS_PORT, TV1_PLUS_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(TV1_MINUS_PORT, TV1_MINUS_PIN, GPIO_INPUT_PULL_UP),

    // VCAN
    GPIO_INIT_FDCAN2RX_PB5,
    GPIO_INIT_FDCAN2TX_PB6,
    // SCAN
    GPIO_INIT_FDCAN3RX_PA8,
    GPIO_INIT_FDCAN3TX_PB4,

    // Throttle
    GPIO_INIT_ANALOG(THROTTLE1_PORT, THROTTLE1_PIN),
    GPIO_INIT_ANALOG(THROTTLE2_PORT, THROTTLE2_PIN),

    // Brake
    GPIO_INIT_ANALOG(REGEN1_PORT, REGEN1_PIN),
    GPIO_INIT_ANALOG(REGEN2_PORT, REGEN2_PIN),

    // Brake Pressure
    GPIO_INIT_ANALOG(BRAKE1_PRESSURE_PORT, BRAKE1_PRESSURE_PIN),
    GPIO_INIT_ANALOG(BRAKE2_PRESSURE_PORT, BRAKE2_PRESSURE_PIN),

    // LCD
    GPIO_INIT_USART1TX_PA9,
    GPIO_INIT_USART1RX_PA10,
};

/* ADC Configuration */
static const PHAL_ADC_ChannelConfig_t adc_channels[] = {
    {.channel = THROTTLE1_ADC_CHANNEL},
    {.channel = THROTTLE2_ADC_CHANNEL},
    {.channel = REGEN1_ADC_CHANNEL},
    {.channel = REGEN2_ADC_CHANNEL},
    {.channel = BRAKE1_PRESSURE_ADC_CHANNEL},
    {.channel = BRAKE2_PRESSURE_ADC_CHANNEL},
};
static const PHAL_ADC_Config_t adc_config = {
    .instance      = ADC1,
    .channels      = adc_channels,
    .channel_count = sizeof(adc_channels) / sizeof(adc_channels[0]),
};

static_assert(
    (sizeof(raw_adc_values_t) / sizeof(uint16_t)) ==
    (sizeof(adc_channels) / sizeof(adc_channels[0])),
    "ADC channel config and raw ADC values struct must have the same number of channels"
);

volatile raw_adc_values_t raw_adc_values; // DMA target
PHAL_ADC_Handle_t adc_handle;

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
RTOS_DEFINE_TASK(report_telemetry_02hz, TELEMETRY_02HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
// RTOS_DEFINE_TASK(calibrate_LWS, 0, TASK_PRIORITY_LOW, STACK_512); // ! only enable for calibration
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(sweep_external_leds);

int main(void) {
    // Hardware Initialization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (false == PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (false == PHAL_USART_init(LCD_USART, LCD_BAUD_RATE, PHAL_RCC_getAPB2ClockHz())) {
        HardFault_Handler();
    }
    if (false == PHAL_ADC_init(&adc_handle, &adc_config)) {
        HardFault_Handler();
    }
    if (!PHAL_ADC_readDMA(&adc_handle, (uint16_t *)&raw_adc_values,
                          sizeof(raw_adc_values) / sizeof(uint16_t))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN3, SCAN_BAUD_RATE);
    CAN_init();

    // Software Initialization
    START_CAN_TASKS();
    CAN_SEND_dash_init(WDG_get_CSR());
    RTOS_START_TASK(pedals_periodic);
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(driver_interface_periodic);
    RTOS_START_TASK(report_telemetry_02hz);
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
            PHAL_writeGPIO(PRCHG_LED_PORT, PRCHG_LED_PIN, 1);
            PHAL_writeGPIO(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_writeGPIO(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_writeGPIO(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 1:
            PHAL_writeGPIO(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_writeGPIO(IMD_LED_PORT, IMD_LED_PIN, 1);
            PHAL_writeGPIO(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_writeGPIO(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 2:
            PHAL_writeGPIO(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_writeGPIO(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_writeGPIO(BMS_LED_PORT, BMS_LED_PIN, 1);
            PHAL_writeGPIO(REGEN_LED_PORT, REGEN_LED_PIN, 0);
            break;
        case 3:
            PHAL_writeGPIO(PRCHG_LED_PORT, PRCHG_LED_PIN, 0);
            PHAL_writeGPIO(IMD_LED_PORT, IMD_LED_PIN, 0);
            PHAL_writeGPIO(BMS_LED_PORT, BMS_LED_PIN, 0);
            PHAL_writeGPIO(REGEN_LED_PORT, REGEN_LED_PIN, 1);
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
    ERROR_LED_PORT->BSRR = (1 << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
