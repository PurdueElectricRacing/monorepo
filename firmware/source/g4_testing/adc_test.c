/**
 * @file adc_test.c
 * @brief G4 ADC loopback test verifying end-to-end conversion accuracy.
 *
 * PA4 (DAC1_OUT1) must be wired to PA0 (ADC1_IN1). The test sweeps 12-bit DAC
 * codes, reads each back from the continuously sampled ADC buffer, and checks the
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
 * red LED indicates an initialization or conversion failure.
 */

static constexpr uint32_t ADC_TEST_DAC_SETTLE_CYCLES  = 10'000U;
static constexpr uint16_t ADC_TEST_ALLOWED_ERROR      = 48U;

static PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_ANALOG(GPIOA, 4),
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

static const PHAL_ADC_InitConfig_t adc_config[] = {
    PHAL_ADC_INIT_ON(GPIOA, 0, ADC1),
};

static const uint16_t dac_test_codes[] = {
    256U,
    1024U,
    2048U,
    3072U,
    3840U,
};

volatile uint16_t adc_test_expected;
volatile uint16_t adc_test_actual;
volatile uint16_t adc_test_error;
volatile uint32_t adc_test_failed_index = UINT32_MAX;
volatile bool adc_test_passed;

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

static bool run_adc_dac_test(void) {
    for (uint32_t i = 0U; i < countof(dac_test_codes); i++) {
        adc_test_expected = dac_test_codes[i];
        dac_write(adc_test_expected);

        adc_test_actual = PHAL_ADC_read(GPIOA, 0);
        adc_test_error  = sample_error(adc_test_expected, adc_test_actual);
        if (adc_test_actual == PHAL_ADC_INVALID_READING || adc_test_error > ADC_TEST_ALLOWED_ERROR) {
            adc_test_failed_index = i;
            return false;
        }
    }
    return true;
}

int main(void) {
    // 16 MHz system clock, no PLL
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    dac_init();
    if (!PHAL_ADC_init(adc_config, countof(adc_config))) {
        HardFault_Handler();
    }

    adc_test_passed = run_adc_dac_test();
    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, adc_test_passed);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, !adc_test_passed);

    while (true) {
        __NOP();
    }
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_ADC
