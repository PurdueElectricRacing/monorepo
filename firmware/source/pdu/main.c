/**
 * @file main.c
 * @brief "PDU" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/faults_common.h"
#include "can_library/generated/PDU.h"
#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/adc/adc.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"
#include "common/heartbeat/heartbeat.h"

/* Module Includes */
#include "cooling_fsm.h"
#include "current_sense.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Status indicators
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Switch control lines
    PHAL_GPIO_INIT_OUTPUT(CTRL1_PORT, CTRL1_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL2_PORT, CTRL2_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL3_PORT, CTRL3_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL4_PORT, CTRL4_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL5_PORT, CTRL5_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL6_PORT, CTRL6_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL7_PORT, CTRL7_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL8_PORT, CTRL8_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL9_PORT, CTRL9_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL10_PORT, CTRL10_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL11_PORT, CTRL11_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL12_PORT, CTRL12_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL13_PORT, CTRL13_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL14_PORT, CTRL14_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL15_PORT, CTRL15_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL16_PORT, CTRL16_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL17_PORT, CTRL17_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL18_PORT, CTRL18_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL19_PORT, CTRL19_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL20_PORT, CTRL20_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CTRL21_PORT, CTRL21_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Analog inputs
    PHAL_GPIO_INIT_ANALOG(LVBATT_VS_PORT, LVBATT_VS_PIN),
    PHAL_GPIO_INIT_ANALOG(LVBATT_CS_PORT, LVBATT_CS_PIN),
    PHAL_GPIO_INIT_ANALOG(MUX1_OUT_PORT, MUX1_OUT_PIN),
    PHAL_GPIO_INIT_ANALOG(MUX2_OUT_PORT, MUX2_OUT_PIN),

    // Analog multiplexer controls
    PHAL_GPIO_INIT_OUTPUT(MUX1_CTRL_A_PORT, MUX1_CTRL_A_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX1_CTRL_B_PORT, MUX1_CTRL_B_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX1_CTRL_C_PORT, MUX1_CTRL_C_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX2_CTRL_A_PORT, MUX2_CTRL_A_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX2_CTRL_B_PORT, MUX2_CTRL_B_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX2_CTRL_C_PORT, MUX2_CTRL_C_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(MUX2_CTRL_D_PORT, MUX2_CTRL_D_PIN, GPIO_OUTPUT_LOW_SPEED),

    // VCAN
    PHAL_PIN_DEFS_FDCAN1_TX_PA12,
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,

    // SCAN
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
    PHAL_PIN_DEFS_FDCAN2_RX_PB12
};

// !! ADC PHAL this api sucks
static const PHAL_ADC_ChannelConfig_t adc1_channels[] = {
    {.channel = LVBATT_VS_ADC_CHANNEL}
};

static const PHAL_ADC_ChannelConfig_t adc2_channels[] = {
    {.channel = MUX1_OUT_ADC_CHANNEL},
    {.channel = MUX2_OUT_ADC_CHANNEL}
};

static const PHAL_ADC_ChannelConfig_t adc3_channels[] = {
    {.channel = LVBATT_CS_ADC_CHANNEL}
};

static const PHAL_ADC_Config_t adc1_config = {
    .instance      = ADC1,
    .channels      = adc1_channels,
    .channel_count = sizeof(adc1_channels) / sizeof(adc1_channels[0]),
};

static const PHAL_ADC_Config_t adc2_config = {
    .instance      = ADC2,
    .channels      = adc2_channels,
    .channel_count = sizeof(adc2_channels) / sizeof(adc2_channels[0]),
};

static const PHAL_ADC_Config_t adc3_config = {
    .instance      = ADC3,
    .channels      = adc3_channels,
    .channel_count = sizeof(adc3_channels) / sizeof(adc3_channels[0]),
};

volatile raw_adc1_values_t raw_adc1_values;
volatile raw_adc2_values_t raw_adc2_values;
volatile raw_adc3_values_t raw_adc3_values;

static PHAL_ADC_Handle_t adc1_handle;
static PHAL_ADC_Handle_t adc2_handle;
static PHAL_ADC_Handle_t adc3_handle;

void HardFault_Handler();

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(fault_library_periodic, PDU_FAULT_SYNC_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_1024);
RTOS_DEFINE_TASK(cooling_fsm_periodic, COOLING_FSM_PERIOD_MS, TASK_PRIORITY_LOW, STACK_1024);
RTOS_DEFINE_TASK(current_sense_periodic, CURRENT_SENSE_TASK_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(SWITCH_enable_all, 0, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main() {
    // Hardware Initialization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);
    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    PHAL_ADC_init(&adc1_handle, &adc1_config);
    PHAL_ADC_init(&adc2_handle, &adc2_config);
    PHAL_ADC_init(&adc3_handle, &adc3_config);
    if (!PHAL_ADC_readDMA(&adc1_handle, (uint16_t *)&raw_adc1_values, 1U)
        || !PHAL_ADC_readDMA(&adc2_handle, (uint16_t *)&raw_adc2_values, 2U)
        || !PHAL_ADC_readDMA(&adc3_handle, (uint16_t *)&raw_adc3_values, 1U)) {
        HardFault_Handler();
    }

    CAN_init();

    // Disable all switches immediately on startup
    // SWITCH_enable_all will do the proper enabling of switches in a safe order
    for (switch_id_t id = 0; id < NUM_SWITCHES; id++) {
        SWITCH_disable(id);
    }

    START_CAN_TASKS();
    CAN_SEND_pdu_init(WDG_get_CSR());
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(current_sense_periodic);
    RTOS_START_TASK(cooling_fsm_periodic);
    RTOS_START_TASK(SWITCH_enable_all);
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
