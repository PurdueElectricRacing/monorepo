/**
 * @file adc_test.c
 * @brief G4 ADC loopback test verifying end-to-end conversion accuracy.
 *
 * PA4 (DAC1_OUT1) must be wired to PA0 (ADC1_IN1). The test sweeps 12-bit DAC
 * codes, reads each back with an asynchronous DMA conversion, and checks the
 * error against a fixed tolerance. Green LED = pass, red LED = failure.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_ADC)

#include <stdbool.h>
#include <stdint.h>

#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/utils/countof.h"
#include "main.h"

/**
 * Connect PA4 (DAC1_OUT1) to PA0 (ADC1_IN1) before running this test.
 * The green LED indicates that every DAC code was read within tolerance; the
 * red LED indicates initialization, timeout, DMA, or conversion failure.
 */

static constexpr uint32_t ADC_TEST_DMA_TIMEOUT        = 1'000'000U;
static constexpr uint32_t ADC_TEST_DAC_SETTLE_CYCLES  = 10'000U;
static constexpr uint16_t ADC_TEST_ALLOWED_ERROR      = 48U;
static constexpr uint8_t ADC_TEST_INPUT_CHANNEL       = 1U;

static PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_ANALOG(GPIOA, 0),
    PHAL_GPIO_INIT_ANALOG(GPIOA, 4),
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

static const PHAL_ADC_ChannelConfig_t adc_channels[] = {
    {.channel = ADC_TEST_INPUT_CHANNEL},
};

static const PHAL_ADC_Config_t adc_config = {
    .instance      = ADC1,
    .channels      = adc_channels,
    .channel_count = sizeof(adc_channels) / sizeof(adc_channels[0]),
};

static const uint16_t dac_test_codes[] = {
    256U,
    1024U,
    2048U,
    3072U,
    3840U,
};

static PHAL_ADC_Handle_t adc_handle;
static uint16_t adc_sample;

volatile uint16_t adc_test_expected;
volatile uint16_t adc_test_actual;
volatile uint16_t adc_test_error;
volatile uint32_t adc_test_failed_index = UINT32_MAX;
volatile bool adc_test_passed;

static volatile bool adc_test_conversion_complete;
static volatile bool adc_test_conversion_error;
static uint32_t adc_test_index;
static uint32_t adc_test_wait_iterations;
static bool adc_test_conversion_started;

typedef enum {
    ADC_TEST_RUNNING,
    ADC_TEST_PASS,
    ADC_TEST_FAIL,
} adc_test_result_t;

void HardFault_Handler(void);

static void dac_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_DAC1EN;
    (void)RCC->AHB2ENR;

    DAC1->CR &= ~(DAC_CR_EN1 | DAC_CR_TEN1);
    DAC1->MCR &= ~DAC_MCR_MODE1_Msk;
    DAC1->DHR12R1 = 0U;
    DAC1->CR |= DAC_CR_EN1;
}

static void dac_write(uint16_t value) {
    DAC1->DHR12R1 = value & DAC_DHR12R1_DACC1DHR_Msk;

    for (volatile uint32_t i = 0U; i < ADC_TEST_DAC_SETTLE_CYCLES; i++) {
        __NOP();
    }
}

static uint16_t sample_error(uint16_t expected, uint16_t actual) {
    return expected > actual ? expected - actual : actual - expected;
}

void PHAL_ADC_conversionCompleteCallback(PHAL_ADC_Handle_t *handle) {
    if (handle != &adc_handle) {
        return;
    }

    adc_test_conversion_error    = handle->transfer_error;
    adc_test_conversion_complete = true;
}

static bool start_adc_dac_test(void) {
    adc_test_expected = dac_test_codes[adc_test_index];
    dac_write(adc_test_expected);

    adc_test_conversion_complete = false;
    adc_test_conversion_error    = false;
    adc_test_wait_iterations     = 0U;

    if (!PHAL_ADC_readDMA(&adc_handle, &adc_sample, 1U)) {
        adc_test_failed_index = adc_test_index;
        return false;
    }

    adc_test_conversion_started = true;
    return true;
}

static adc_test_result_t run_adc_dac_test_step(void) {
    if (!adc_test_conversion_started) {
        return start_adc_dac_test() ? ADC_TEST_RUNNING : ADC_TEST_FAIL;
    }

    if (!adc_test_conversion_complete) {
        adc_test_wait_iterations++;
        if (adc_test_wait_iterations >= ADC_TEST_DMA_TIMEOUT) {
            adc_test_failed_index = adc_test_index;
            return ADC_TEST_FAIL;
        }
        return ADC_TEST_RUNNING;
    }

    adc_test_conversion_complete = false;
    if (adc_test_conversion_error) {
        adc_test_failed_index = adc_test_index;
        return ADC_TEST_FAIL;
    }

    adc_test_actual = adc_sample;
    adc_test_error  = sample_error(adc_test_expected, adc_test_actual);
    if (adc_test_error > ADC_TEST_ALLOWED_ERROR) {
        adc_test_failed_index = adc_test_index;
        return ADC_TEST_FAIL;
    }

    adc_test_index++;
    if (adc_test_index >= countof(dac_test_codes)) {
        return ADC_TEST_PASS;
    }

    adc_test_expected      = dac_test_codes[adc_test_index];
    adc_test_conversion_error = false;
    adc_test_wait_iterations  = 0U;
    dac_write(adc_test_expected);
    return ADC_TEST_RUNNING;
}

int main(void) {
    // 16 MHz system clock, no PLL
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    dac_init();
    if (!PHAL_ADC_init(&adc_handle, &adc_config)) {
        HardFault_Handler();
    }

    adc_test_result_t result = ADC_TEST_RUNNING;
    while (true) {
        if (result == ADC_TEST_RUNNING) {
            result = run_adc_dac_test_step();
            if (result != ADC_TEST_RUNNING) {
                adc_test_passed = result == ADC_TEST_PASS;
                PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, adc_test_passed);
                PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, !adc_test_passed);
            }
        }
        __NOP();
    }
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_ADC
