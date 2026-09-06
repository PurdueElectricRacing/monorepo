/**
 * @file main.c
 * @brief "Driveline" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Anya Pokrovskaya (apokrovs@purdue.edu)
 */

/* System Includes */
#include "can_library/generated/DRIVELINE.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/fdcan/fdcan.h"
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

    // VCAN
    PHAL_PIN_DEFS_FDCAN2_RX_PB5,
    PHAL_PIN_DEFS_FDCAN2_TX_PB6,

    // Shock Pots
    PHAL_GPIO_INIT_ANALOG(SHOCKPOT_LEFT_GPIO_PORT , SHOCKPOT_LEFT_GPIO_PIN),
    PHAL_GPIO_INIT_ANALOG(SHOCKPOT_RIGHT_GPIO_PORT, SHOCKPOT_RIGHT_GPIO_PIN),

    //Oil temps
    PHAL_GPIO_INIT_ANALOG(OIL_TEMP_L_GPIO_Port, OIL_TEMP_L_Pin),
    PHAL_GPIO_INIT_ANALOG(OIL_TEMP_R_GPIO_Port, OIL_TEMP_R_Pin),

};

/* ADC Configuration */

// ADC 1
static const PHAL_ADC_ChannelConfig_t adc1_channels[] = {
    {.channel = OIL_TEMP_L_ADC_CH},
};
static const PHAL_ADC_Config_t adc1_config = {
    .instance      = ADC1,
    .channels      = adc1_channels,
    .channel_count = sizeof(adc1_channels) / sizeof(adc1_channels[0]),
};
typedef struct {
    uint16_t oil_temp_left;
} raw_adc1_values_t;
volatile raw_adc1_values_t raw_adc1_values;
static PHAL_ADC_Handle_t adc1_handle;


// ADC 2
static const PHAL_ADC_ChannelConfig_t adc2_channels[] = {
    {.channel = OIL_TEMP_R_ADC_CH},
};
static const PHAL_ADC_Config_t adc2_config = {
    .instance      = ADC2,
    .channels      = adc2_channels,
    .channel_count = sizeof(adc2_channels) / sizeof(adc2_channels[0]),
};
typedef struct {
    uint16_t oil_temp_right;
} raw_adc2_values_t;
volatile raw_adc2_values_t raw_adc2_values;
static PHAL_ADC_Handle_t adc2_handle;


// ADC 3

static const PHAL_ADC_ChannelConfig_t adc3_channels[] = {
    {.channel = SHOCKPOT_LEFT_ADC_CHNL},
};
static const PHAL_ADC_Config_t adc3_config = {
    .instance      = ADC3,
    .channels      = adc3_channels,
    .channel_count = sizeof(adc3_channels) / sizeof(adc3_channels[0]),
};
typedef struct {
    uint16_t shock_l;
} raw_adc3_values_t;
volatile raw_adc3_values_t raw_adc3_values;
static PHAL_ADC_Handle_t adc3_handle;

// ADC 4

static const PHAL_ADC_ChannelConfig_t adc4_channels[] = {
    {.channel = SHOCKPOT_RIGHT_ADC_CHNL},
};
static const PHAL_ADC_Config_t adc4_config = {
    .instance      = ADC4,
    .channels      = adc4_channels,
    .channel_count = sizeof(adc4_channels) / sizeof(adc4_channels[0]),
};
typedef struct {
    uint16_t shock_r;
} raw_adc4_values_t;
volatile raw_adc4_values_t raw_adc4_values;
static PHAL_ADC_Handle_t adc4_handle;


// note: the raw_*_values structs are the DMA destinations for each ADC's
// read, so their layout must match the channel order and they must be
// uint16_t-aligned (a single uint16_t member is both)


extern void HardFault_Handler();
void shockpots_periodic();
void oil_temps_periodic();

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(shockpots_periodic, FRONT_SHOCKPOTS_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(oil_temps_periodic, FRONT_OIL_TEMPS_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main(void) {
    // Hardware Initilization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (false == PHAL_ADC_init(&adc1_handle, &adc1_config)) {
        HardFault_Handler();
    }
    if (false == PHAL_ADC_init(&adc2_handle, &adc2_config)) {
        HardFault_Handler();
    }
    if (false == PHAL_ADC_init(&adc3_handle, &adc3_config)) {
        HardFault_Handler();
    }
    if (false == PHAL_ADC_init(&adc4_handle, &adc4_config)) {
        HardFault_Handler();
    }
    if (!PHAL_ADC_readDMA(&adc1_handle, (uint16_t *)&raw_adc1_values, 1U)
        || !PHAL_ADC_readDMA(&adc2_handle, (uint16_t *)&raw_adc2_values, 1U)
        || !PHAL_ADC_readDMA(&adc3_handle, (uint16_t *)&raw_adc3_values, 1U)
        || !PHAL_ADC_readDMA(&adc4_handle, (uint16_t *)&raw_adc4_values, 1U)) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    CAN_init();

    // Software Initalization
    START_CAN_TASKS();
    SEND_INIT(WDG_get_CSR());
    RTOS_START_TASK(shockpots_periodic);
    RTOS_START_TASK(oil_temps_periodic);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

// globals for GDB
uint16_t left_length_scaled = 0;
uint16_t right_length_scaled = 0;
void shockpots_periodic() {
    static_assert(FRONT_SHOCKPOTS_LAYOUT_HASH == REAR_SHOCKPOTS_LAYOUT_HASH, "Shockpot messages should be the same");
    static constexpr float ADC_MAX         = 4095.0f;
    static constexpr float STROKE_MM       = 75.0f;
    static constexpr float FIXED_LENGTH_MM = 120.0f + 25.0f; // 120mm shock body + 25mm head

    // ! assumes that 4095 is fully compressed and 0 is fully extended
    uint16_t inverted_shock_l = 4095 - raw_adc3_values.shock_l;
    uint16_t inverted_shock_r = 4095 - raw_adc4_values.shock_r;
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

    float left_volts  = raw_adc1_values.oil_temp_left * ADC_TO_VOLTS;
    float right_volts = raw_adc2_values.oil_temp_right * ADC_TO_VOLTS;

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
