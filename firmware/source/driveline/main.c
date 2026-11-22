/**
 * @file main.c
 * @brief "Driveline" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Anya Pokrovskaya (apokrovs@purdue.edu)
 */

/* System Includes */
#include "can_library/generated/DRIVELINE.h"
#include "can_library/generated/can_version.h"
#include "common/bootloader/bootloader_common.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "config.h"
#include "oil_temps_table.h"
#include "pin_defs.h"

/* PER HAL Initilization Structures */
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Status LEDs
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // SCAN
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12,
};

static const PHAL_ADC_InitConfig_t adc_config[] = {
    PHAL_ADC_INIT(SHOCKPOT_LEFT_GPIO_PORT, SHOCKPOT_LEFT_GPIO_PIN),
    PHAL_ADC_INIT(SHOCKPOT_RIGHT_GPIO_PORT, SHOCKPOT_RIGHT_GPIO_PIN),
    PHAL_ADC_INIT(OIL_TEMP_L_GPIO_Port, OIL_TEMP_L_Pin),
    PHAL_ADC_INIT(OIL_TEMP_R_GPIO_Port, OIL_TEMP_R_Pin),
};

extern void HardFault_Handler();
void shockpots_periodic();
void oil_temps_periodic();
void report_telemetry_02hz();

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(shockpots_periodic, FRONT_SHOCKPOTS_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(oil_temps_periodic, FRONT_OIL_TEMPS_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_02hz, FRONT_DRIVELINE_VERSION_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main(void) {
    // Hardware Initilization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (!PHAL_ADC_init(adc_config, countof(adc_config))) {
        HardFault_Handler();
    }

    CAN_init();

    // Software Initalization
    START_CAN_TASKS();
    SEND_INIT(WDG_get_CSR());
    RTOS_START_TASK(shockpots_periodic);
    RTOS_START_TASK(oil_temps_periodic);
    RTOS_START_TASK(report_telemetry_02hz);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

// globals for GDB
uint16_t left_length_scaled = 0;
uint16_t right_length_scaled = 0;
static_assert(FRONT_DRIVELINE_VERSION_PERIOD_MS == 5000U);
static_assert(REAR_DRIVELINE_VERSION_PERIOD_MS == 5000U);
static_assert(FRONT_DRIVELINE_CAN_OVERFLOW_PERIOD_MS == 5000U);
static_assert(FRONT_DRIVELINE_CAN_TX_ACTIONS_PERIOD_MS == 5000U);
static_assert(FRONT_DRIVELINE_CAN_TX_MESSAGES_PERIOD_MS == 5000U);
static_assert(REAR_DRIVELINE_CAN_OVERFLOW_PERIOD_MS == 5000U);
static_assert(REAR_DRIVELINE_CAN_TX_ACTIONS_PERIOD_MS == 5000U);
static_assert(REAR_DRIVELINE_CAN_TX_MESSAGES_PERIOD_MS == 5000U);
void report_telemetry_02hz() {
    SEND_VERSION(FIRMWARE_GIT_HASH, BL_getGitHash(), APPLICATION_BOOTLOADABLE);
    SEND_CAN_OVERFLOW(can_stats.rx_overflow, can_stats.tx_overflow);
    SEND_CAN_TX_ACTIONS(can_stats.tx_enqueue_count, can_stats.tx_task_wake_count);
    SEND_CAN_TX_MESSAGES(can_stats.tx_sent_count, can_stats.tx_callback_count);
}

void shockpots_periodic() {
    static_assert(FRONT_SHOCKPOTS_LAYOUT_HASH == REAR_SHOCKPOTS_LAYOUT_HASH, "Shockpot messages should be the same");
    static constexpr float ADC_MAX         = 4095.0f;
    static constexpr float STROKE_MM       = 75.0f;
    static constexpr float FIXED_LENGTH_MM = 120.0f + 25.0f; // 120mm shock body + 25mm head

    // ! assumes that 4095 is fully compressed and 0 is fully extended
    uint16_t inverted_shock_l = 4095 - PHAL_ADC_read(SHOCKPOT_LEFT_GPIO_PORT, SHOCKPOT_LEFT_GPIO_PIN);
    uint16_t inverted_shock_r = 4095 - PHAL_ADC_read(SHOCKPOT_RIGHT_GPIO_PORT, SHOCKPOT_RIGHT_GPIO_PIN);
    float left_travel  = (inverted_shock_l / ADC_MAX) * STROKE_MM;
    float right_travel = (inverted_shock_r / ADC_MAX) * STROKE_MM;

    float left_length  = left_travel + FIXED_LENGTH_MM;
    float right_length = right_travel + FIXED_LENGTH_MM;

    left_length_scaled  = (uint16_t)(left_length * PACK_COEFF_FRONT_SHOCKPOTS_LEFT);
    right_length_scaled = (uint16_t)(right_length * PACK_COEFF_FRONT_SHOCKPOTS_RIGHT);

#ifdef IS_FRONT_DRIVELINE
    // ! account for error in the harness: front left and right are swapped
    CAN_SEND_front_shockpots(right_length_scaled, left_length_scaled);
#else
    CAN_SEND_rear_shockpots(left_length_scaled, right_length_scaled);
#endif
}

// globals for GDB
int16_t left_celsius_scaled = 0;
int16_t right_celsius_scaled = 0;
void oil_temps_periodic() {
    static_assert(FRONT_OIL_TEMPS_LAYOUT_HASH == REAR_OIL_TEMPS_LAYOUT_HASH, "Oil temp messages should be the same");
    static constexpr float ADC_MAX      = 4095.0f;
    static constexpr float ADC_VREF     = 3.3f;
    static constexpr float ADC_TO_VOLTS = ADC_VREF / ADC_MAX;
    static constexpr float R1           = 220.0f;

    float left_volts  = PHAL_ADC_read(OIL_TEMP_L_GPIO_Port, OIL_TEMP_L_Pin) * ADC_TO_VOLTS;
    float right_volts = PHAL_ADC_read(OIL_TEMP_R_GPIO_Port, OIL_TEMP_R_Pin) * ADC_TO_VOLTS;

    // R_thermistor = (V_out * R1) / (V_ref - V_out)
    float left_resistance  = (left_volts * R1) / (ADC_VREF - left_volts);
    float right_resistance = (right_volts * R1) / (ADC_VREF - right_volts);

    float left_celsius  = oil_temps_R_to_T(left_resistance);
    float right_celsius = oil_temps_R_to_T(right_resistance);

    left_celsius_scaled  = (int16_t)(left_celsius * PACK_COEFF_FRONT_OIL_TEMPS_LEFT);
    right_celsius_scaled = (int16_t)(right_celsius * PACK_COEFF_FRONT_OIL_TEMPS_RIGHT);

    SEND_OIL_TEMPS(left_celsius_scaled, right_celsius_scaled);
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL = 0;
    ERROR_LED_PORT->BSRR = (1U << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
