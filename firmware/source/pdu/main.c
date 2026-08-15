/**
 * @file main.c
 * @brief "PDU" node source code
 *
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Ronak Jain (jain717@purdue.edu)
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/faults_common.h"
#include "can_library/generated/PDU.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_F4/adc/adc.h"
#include "common/phal_F4/can/can.h"
#include "common/phal_F4/dma/dma.h"
#include "common/phal_F4/gpio/gpio.h"
#include "common/phal_F4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "cooling_fsm.h"
#include "faults.h"
#include "flow_rate.h"
#include "led.h"
#include "state.h"
#include "switches.h"
#include "telemetry.h"

GPIOInitConfig_t gpio_config[] = {
    // Status Indicators
    GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    // CAN
    GPIO_INIT_CANRX_PD0,
    GPIO_INIT_CANTX_PD1,
    // MUX Control
    GPIO_INIT_OUTPUT(MUX_CTRL_A_GPIO_Port, MUX_CTRL_A_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(MUX_CTRL_B_GPIO_Port, MUX_CTRL_B_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(MUX_CTRL_C_GPIO_Port, MUX_CTRL_C_Pin, GPIO_OUTPUT_LOW_SPEED),
    // LED CTRL
    GPIO_INIT_SPI1_SCK_PB3,
    GPIO_INIT_SPI1_MOSI_PB5,
    GPIO_INIT_OUTPUT(LED_CTRL_LAT_GPIO_Port, LED_CTRL_LAT_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(LED_CTRL_BLANK_GPIO_Port, LED_CTRL_BLANK_Pin, GPIO_OUTPUT_LOW_SPEED),
    // Flow Rate
    GPIO_INIT_AF(
        FLOW_RATE_1_GPIO_Port,
        FLOW_RATE_1_Pin,
        FLOW_RATE_1_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_AF(
        FLOW_RATE_2_GPIO_Port,
        FLOW_RATE_2_Pin,
        FLOW_RATE_2_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_DOWN),
    // Fan Control
    GPIO_INIT_AF(
        FAN_1_PWM_GPIO_Port,
        FAN_1_PWM_Pin,
        FAN_1_PWM_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_PUSH_PULL,
        GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(
        FAN_2_PWM_GPIO_Port,
        FAN_2_PWM_Pin,
        FAN_2_PWM_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_PUSH_PULL,
        GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(
        FAN_3_PWM_GPIO_Port,
        FAN_3_PWM_Pin,
        FAN_3_PWM_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_PUSH_PULL,
        GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(
        FAN_4_PWM_GPIO_Port,
        FAN_4_PWM_Pin,
        FAN_4_PWM_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_PUSH_PULL,
        GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(
        FAN_1_TACH_GPIO_Port,
        FAN_1_TACH_Pin,
        FAN_1_TACH_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_AF(
        FAN_2_TACH_GPIO_Port,
        FAN_2_TACH_Pin,
        FAN_2_TACH_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_AF(
        FAN_3_TACH_GPIO_Port,
        FAN_3_TACH_Pin,
        FAN_3_TACH_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_UP),
    GPIO_INIT_AF(
        FAN_4_TACH_GPIO_Port,
        FAN_4_TACH_Pin,
        FAN_4_TACH_AF,
        GPIO_OUTPUT_HIGH_SPEED,
        GPIO_OUTPUT_OPEN_DRAIN,
        GPIO_INPUT_PULL_UP),
    // Pump Switches
    GPIO_INIT_OUTPUT(PUMP_1_CTRL_GPIO_Port, PUMP_1_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_ANALOG(PUMP_1_IMON_GPIO_Port, PUMP_1_IMON_Pin),
    GPIO_INIT_OUTPUT(PUMP_2_CTRL_GPIO_Port, PUMP_2_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_ANALOG(PUMP_2_IMON_GPIO_Port, PUMP_2_IMON_Pin),
    // Heat Exchanger Fan
    GPIO_INIT_OUTPUT(HXFAN_CTRL_GPIO_Port, HXFAN_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_ANALOG(HXFAN_IMON_GPIO_Port, HXFAN_IMON_Pin),
    // SDC Switch
    GPIO_INIT_ANALOG(SDC_IMON_GPIO_Port, SDC_IMON_Pin),
    // Fan Switches
    GPIO_INIT_OUTPUT(FAN_3_CTRL_GPIO_Port, FAN_3_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(FAN_4_CTRL_GPIO_Port, FAN_4_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(FAN_1_CTRL_GPIO_Port, FAN_1_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(FAN_2_CTRL_GPIO_Port, FAN_2_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    // Driveline Controls
    GPIO_INIT_OUTPUT(DLFR_CTRL_GPIO_Port, DLFR_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(DLFR_NFLT_GPIO_Port, DLFR_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_ANALOG(DLFR_CS_GPIO_Port, DLFR_CS_Pin),
    GPIO_INIT_OUTPUT(DLBK_CTRL_GPIO_Port, DLBK_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(DLBK_NFLT_GPIO_Port, DLBK_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_ANALOG(DLBK_CS_GPIO_Port, DLBK_CS_Pin),
    // Main Module
    GPIO_INIT_OUTPUT(MAIN_CTRL_GPIO_Port, MAIN_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(MAIN_NFLT_GPIO_Port, MAIN_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_ANALOG(MAIN_CS_GPIO_Port, MAIN_CS_Pin),
    // Dashboard
    GPIO_INIT_INPUT(DASH_NFLT_GPIO_Port, DASH_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_ANALOG(DASH_CS_GPIO_Port, DASH_CS_Pin),
    // ABox
    GPIO_INIT_INPUT(ABOX_NFLT_GPIO_Port, ABOX_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_ANALOG(ABOX_CS_GPIO_Port, ABOX_CS_Pin),
    // Bullet
    GPIO_INIT_OUTPUT(BLT_CTRL_GPIO_Port, BLT_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(BLT_NFLT_GPIO_Port, BLT_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    // 5V Critical Switch
    GPIO_INIT_OUTPUT(CRIT_5V_CTRL_GPIO_Port, CRIT_5V_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(CRIT_5V_NFLT_GPIO_Port, CRIT_5V_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    // 5V Non-Critical Switch
    GPIO_INIT_OUTPUT(TV_CTRL_GPIO_Port, TV_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(TV_NFLT_GPIO_Port, TV_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    // 5V Fan
    GPIO_INIT_OUTPUT(FAN_5V_CTRL_GPIO_Port, FAN_5V_CTRL_Pin, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_INPUT(FAN_5V_NFLT_GPIO_Port, FAN_5V_NFLT_Pin, GPIO_INPUT_OPEN_DRAIN),
    // LV Battery BMS
    GPIO_INIT_INPUT(LV_BMS_STAT_GPIO_Port, LV_BMS_STAT_Pin, GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_USART3TX_PC10,
    GPIO_INIT_USART3RX_PC11,
    // LV Status
    GPIO_INIT_ANALOG(V24_VS_GPIO_Port, V24_VS_Pin),
    GPIO_INIT_ANALOG(V24_CS_GPIO_Port, V24_CS_Pin),
    GPIO_INIT_ANALOG(V5_VS_GPIO_Port, V5_VS_Pin),
    GPIO_INIT_ANALOG(V5_CS_GPIO_Port, V5_CS_Pin),
    GPIO_INIT_ANALOG(V3V3_VS_GPIO_Port, V3V3_VS_Pin),
    GPIO_INIT_ANALOG(MUX_OUT_GPIO_Port, MUX_OUT_Pin),
    GPIO_INIT_ANALOG(DAQ_IMON_GPIO_Port, DAQ_IMON_Pin),
};

/* ADC Configuration */

ADCInitConfig_t adc_config = {
    .clock_prescaler = ADC_CLK_PRESC_6, // Desire ADC clock to be 30MHz (upper bound), clocked from APB2 (160/6=27MHz)
    .resolution      = ADC_RES_12_BIT,
    .data_align      = ADC_DATA_ALIGN_RIGHT,
    .cont_conv_mode  = true,
    .adc_number      = 1,
    .dma_mode        = ADC_DMA_CIRCULAR
};

/* SPI Configuration */
// todo evalaute DMA streams. do we need this? can we use it for ADC?
dma_init_t spi_rx_dma_config = SPI1_RXDMA_CONT_CONFIG(NULL, 2);
dma_init_t spi_tx_dma_config = SPI1_TXDMA_CONT_CONFIG(NULL, 1);
SPI_InitConfig_t spi_config = {
    .data_len   = 8,
    .nss_sw     = false,
    .rx_dma_cfg = &spi_rx_dma_config,
    .tx_dma_cfg = &spi_tx_dma_config,
    .periph     = SPI1
};

/* With 17 items, 16 prescaler, and 640 sample time, each channel gets read every 1.4ms */
volatile ADCReadings_t adc_readings;
ADCChannelConfig_t adc_channel_config[] = {
    {.channel = PUMP_1_IMON_ADC_CHNL, .rank = 1, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = PUMP_2_IMON_ADC_CHNL, .rank = 2, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = HXFAN_IMON_ADC_CHNL, .rank = 3, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = SDC_IMON_ADC_CHNL, .rank = 4, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = DLFR_CS_ADC_CHNL, .rank = 5, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = DLBK_CS_ADC_CHNL, .rank = 6, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = MAIN_CS_ADC_CHNL, .rank = 7, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = DASH_CS_ADC_CHNL, .rank = 8, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = ABOX_CS_ADC_CHNL, .rank = 9, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = DAQ_IMON_ADC_CHNL, .rank = 10, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = V24_VS_ADC_CHNL, .rank = 11, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = V24_CS_ADC_CHNL, .rank = 12, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = V5_VS_ADC_CHNL, .rank = 13, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = V5_CS_ADC_CHNL, .rank = 14, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = V3V3_VS_ADC_CHNL, .rank = 15, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = INTERNAL_THERM_ADC_CHNL, .rank = 16, .sampling_time = ADC_CHN_SMP_CYCLES_480},
    {.channel = MUX_OUT_ADC_CHNL, .rank = 17, .sampling_time = ADC_CHN_SMP_CYCLES_480},
};
dma_init_t adc_dma_config = ADC1_DMA_CONT_CONFIG((uint32_t)&adc_readings,
                                                 sizeof(adc_readings) / sizeof(adc_readings.v24_vs),
                                                 0b01);

#define TargetCoreClockrateHz 16'000'000
ClockRateConfig_t clock_config = {
    .clock_source              = CLOCK_SOURCE_HSE,
    .use_pll                   = false,
    .vco_output_rate_target_hz = 160'000'000,
    .pll_src                   = PLL_SRC_HSE,
    .system_clock_target_hz    = TargetCoreClockrateHz,
    .ahb_clock_target_hz       = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz      = (TargetCoreClockrateHz / (1)),
    .apb2_clock_target_hz      = (TargetCoreClockrateHz / (1)),
};

extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;
extern uint32_t AHBClockRateHz;
extern uint32_t PLLClockRateHz;

void HardFault_Handler();

static void heartbeat_led_sweep(void) {
    static int led_index = 0;
    static bool decrement = false;

    if (!decrement) {
        LED_control(led_index, LED_ON);
        led_index++;
        if (led_index >= MAX_NUM_LED) {
            led_index = MAX_NUM_LED - 1;
            decrement = true;
        }
    } else {
        LED_control(led_index, LED_OFF);
        if (led_index == 0) {
            decrement = false;
        } else {
            led_index--;
        }
    }
}

static void tsal_delayed_start() {
    RTOS_delay_ms(1000);
    switches_set_state(SW_MAIN, true);
    vTaskDelete(NULL);
}

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(switches_periodic, 15, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(tsal_delayed_start, 1000, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(cooling_fsm_periodic, COOLING_FSM_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_1024);
RTOS_DEFINE_TASK(LED_periodic, 500, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(faults_periodic, 100, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(fault_library_periodic, 100, TASK_PRIORITY_LOW, STACK_1024);
RTOS_DEFINE_TASK(telemetry_10hz, 100, TASK_PRIORITY_LOW, STACK_1024);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(heartbeat_led_sweep);

int main() {
    // Hardware Initialization
    if (0 != PHAL_configureClockRates(&clock_config)) {
        HardFault_Handler();
    }
    WDG_init();
    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (!PHAL_initADC(ADC1, &adc_config, adc_channel_config, countof(adc_channel_config))) {
        HardFault_Handler();
    }
    if (!PHAL_initDMA(&adc_dma_config)) {
        HardFault_Handler();
    }
    PHAL_DMA_startTxfer(&adc_dma_config);
    PHAL_startADC(ADC1);

    if (!PHAL_initCAN(CAN1, false, VCAN_BAUD_RATE)) {
        HardFault_Handler();
    }

    CAN_init();

    if (!PHAL_SPI_init(&spi_config)) {
        HardFault_Handler();
    }

    PHAL_writeGPIO(LED_CTRL_BLANK_GPIO_Port, LED_CTRL_BLANK_Pin, 1);

    state_init_defaults();
    switches_init();
    faults_init();

    flowRateInit();

    switches_enable_default_rails();

    START_CAN_TASKS();
    CAN_SEND_pdu_init(WDG_get_CSR());
    RTOS_START_TASK(switches_periodic);
    RTOS_START_TASK(tsal_delayed_start);
    RTOS_START_TASK(cooling_fsm_periodic);
    RTOS_START_TASK(LED_periodic);
    RTOS_START_TASK(faults_periodic);
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(telemetry_10hz);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL = 0;
    ERROR_LED_PORT->BSRR = (1 << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
