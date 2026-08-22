/**
 * @file adc_priv.c
 * @brief Register-level implementation of the STM32G4 ADC PHAL.
 *
 * All direct ADC register manipulation lives here so the public layer only
 * coordinates the ADC with its DMA channel. Covers clocking, calibration,
 * sequence and sampling-time programming, and bounded busy-wait start/stop
 * helpers.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "common/phal_G4/adc/adc_priv.h"

/// HSI16 rate, used only as a fallback when SystemCoreClock is unset
static constexpr uint32_t ADC_PRIV_HSI_CLOCK_RATE_HZ = 16'000'000U;

static constexpr uint32_t ADC_PRIV_WAIT_ITERATIONS = 100'000U;
static constexpr uint32_t ADC_PRIV_SAMPLE_TIME_MAX = 7U;

/// Return the common register bank containing an ADC instance.
static ADC_Common_TypeDef *adc_common_registers(const ADC_TypeDef *instance) {
    return (instance == ADC1 || instance == ADC2) ? ADC12_COMMON : ADC345_COMMON;
}

/// Enable and synchronize the peripheral clock for an ADC instance.
static void adc_enable_clock(const ADC_TypeDef *instance) {
    if (instance == ADC1 || instance == ADC2) {
        RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
    } else {
        RCC->AHB2ENR |= RCC_AHB2ENR_ADC345EN;
    }
    (void)RCC->AHB2ENR;
}

/// Select the synchronous HCLK/4 ADC clock if it is not already configured.
static void adc_configure_clock(const ADC_TypeDef *instance) {
    // CKMODE = 0b11 selects HCLK / 4. In synchronous mode PRESC is unused.
    // Avoid rewriting the shared ADC12/ADC345 common clock while a sibling ADC
    // may already be enabled.
    ADC_Common_TypeDef *common = adc_common_registers(instance);
    uint32_t clock_bits = common->CCR & (ADC_CCR_CKMODE_Msk | ADC_CCR_PRESC_Msk);
    if (clock_bits == ADC_CCR_CKMODE_Msk) {
        return;
    }

    uint32_t ccr = common->CCR;
    ccr &= ~(ADC_CCR_CKMODE_Msk | ADC_CCR_PRESC_Msk);
    ccr |= ADC_CCR_CKMODE_Msk;
    common->CCR = ccr;
}

/// Wait for masked register bits to reach the requested state.
static bool adc_wait_for_register(
    const volatile uint32_t *reg,
    uint32_t mask,
    bool expected_set
) {
    uint32_t iterations_remaining = ADC_PRIV_WAIT_ITERATIONS;
    while (iterations_remaining > 0U) {
        if (((*reg & mask) != 0U) == expected_set) {
            return true;
        }
        iterations_remaining--;
    }
    return false;
}

/// Wait at least the ADC regulator startup interval.
static void adc_regulator_delay(void) {
    uint32_t clock_hz = SystemCoreClock;
    if (clock_hz == 0U) {
        clock_hz = ADC_PRIV_HSI_CLOCK_RATE_HZ;
    }

    uint32_t cycles = clock_hz / 50'000U;
    if (cycles == 0U) {
        cycles = 1U;
    }

    for (volatile uint32_t i = 0U; i < cycles; i++) {
        __NOP();
    }
}

/// Program one channel into a regular conversion sequence rank.
static void adc_set_sequence_slot(ADC_TypeDef *instance, uint8_t rank, uint8_t channel) {
    volatile uint32_t *sequence_register;
    uint32_t shift;

    if (rank <= 4U) {
        sequence_register = &instance->SQR1;
        shift             = 6U + 6U * (rank - 1U);
    } else if (rank <= 9U) {
        sequence_register = &instance->SQR2;
        shift             = 6U * (rank - 5U);
    } else if (rank <= 14U) {
        sequence_register = &instance->SQR3;
        shift             = 6U * (rank - 10U);
    } else {
        sequence_register = &instance->SQR4;
        shift             = 6U * (rank - 15U);
    }

    *sequence_register = (*sequence_register & ~(0x1FU << shift))
        | ((uint32_t)channel << shift);
}

/// Set one channel to the driver's fixed maximum sampling time.
static void adc_set_sample_time(ADC_TypeDef *instance, uint8_t channel) {
    volatile uint32_t *sample_register =
        channel <= 9U ? &instance->SMPR1 : &instance->SMPR2;
    uint32_t sample_index = channel <= 9U ? channel : channel - 10U;
    uint32_t shift        = 3U * sample_index;

    *sample_register = (*sample_register & ~(0x7U << shift))
        | (ADC_PRIV_SAMPLE_TIME_MAX << shift);
}

/// Program the configured channel order and sampling times.
static void adc_configure_sequence(
    ADC_TypeDef *instance,
    const PHAL_ADC_ChannelConfig_t *channels,
    size_t channel_count
) {
    instance->SQR1 =
        ((uint32_t)(channel_count - 1U) << ADC_SQR1_L_Pos) & ADC_SQR1_L_Msk;
    instance->SQR2  = 0U;
    instance->SQR3  = 0U;
    instance->SQR4  = 0U;
    instance->SMPR1 = 0U;
    instance->SMPR2 = 0U;

    for (size_t i = 0U; i < channel_count; i++) {
        adc_set_sequence_slot(instance, (uint8_t)(i + 1U), channels[i].channel);
        adc_set_sample_time(instance, channels[i].channel);
    }
}

/// Apply the driver's fixed resolution, trigger, and DMA conversion settings.
static void adc_configure_conversion(ADC_TypeDef *instance) {
    uint32_t cfgr = instance->CFGR;
    cfgr &= ~(ADC_CFGR_CONT | ADC_CFGR_DISCEN | ADC_CFGR_DMAEN
              | ADC_CFGR_DMACFG | ADC_CFGR_RES_Msk | ADC_CFGR_ALIGN
              | ADC_CFGR_EXTSEL_Msk | ADC_CFGR_EXTEN_Msk);
    cfgr |= ADC_CFGR_DMAEN | ADC_CFGR_CONT | ADC_CFGR_DMACFG;
    instance->CFGR = cfgr;

    instance->CFGR2 &= ~(ADC_CFGR2_ROVSE | ADC_CFGR2_OVSR_Msk | ADC_CFGR2_OVSS_Msk);
}

/// Run single-ended ADC calibration and wait for completion.
static bool adc_calibrate(ADC_TypeDef *instance) {
    instance->CR &= ~ADC_CR_ADCALDIF;
    instance->CR |= ADC_CR_ADCAL;
    return adc_wait_for_register(&instance->CR, ADC_CR_ADCAL, false);
}

bool ADC_PRIV_instance_is_supported(const ADC_TypeDef *instance) {
    return instance == ADC1 || instance == ADC2 || instance == ADC3 || instance == ADC4;
}

bool ADC_PRIV_configure(
    ADC_TypeDef *instance,
    const PHAL_ADC_ChannelConfig_t *channels,
    size_t channel_count
) {
    adc_enable_clock(instance);
    if (!ADC_PRIV_disable(instance)) {
        return false;
    }
    adc_configure_clock(instance);
    instance->CR &= ~ADC_CR_DEEPPWD;
    instance->CR |= ADC_CR_ADVREGEN;
    adc_regulator_delay();

    if (!adc_calibrate(instance)) {
        return false;
    }

    adc_configure_conversion(instance);
    adc_configure_sequence(instance, channels, channel_count);
    instance->IER = 0U;
    return true;
}

bool ADC_PRIV_enable(ADC_TypeDef *instance) {
    instance->ISR = ADC_ISR_ADRDY | ADC_ISR_EOSMP | ADC_ISR_EOC
        | ADC_ISR_EOS | ADC_ISR_OVR;
    instance->CR |= ADC_CR_ADEN;
    return adc_wait_for_register(&instance->ISR, ADC_ISR_ADRDY, true);
}

bool ADC_PRIV_disable(ADC_TypeDef *instance) {
    if (!ADC_PRIV_stop_conversion(instance)) {
        return false;
    }
    if ((instance->CR & ADC_CR_ADEN) == 0U) {
        return true;
    }

    instance->CR |= ADC_CR_ADDIS;
    return adc_wait_for_register(&instance->CR, ADC_CR_ADEN, false);
}

bool ADC_PRIV_is_ready(const ADC_TypeDef *instance) {
    return (instance->CR & ADC_CR_ADEN) != 0U
        && (instance->ISR & ADC_ISR_ADRDY) != 0U;
}

void ADC_PRIV_prepare_transfer(ADC_TypeDef *instance, bool continuous) {
    uint32_t cfgr = instance->CFGR & ~(ADC_CFGR_CONT | ADC_CFGR_DMACFG);
    if (continuous) {
        cfgr |= ADC_CFGR_CONT | ADC_CFGR_DMACFG;
    }
    instance->CFGR = cfgr;
    instance->ISR  = ADC_ISR_EOSMP | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
}

void ADC_PRIV_start_conversion(ADC_TypeDef *instance) {
    instance->CR |= ADC_CR_ADSTART;
}

bool ADC_PRIV_stop_conversion(ADC_TypeDef *instance) {
    if ((instance->CR & ADC_CR_ADSTART) == 0U) {
        return true;
    }

    instance->CR |= ADC_CR_ADSTP;
    return adc_wait_for_register(&instance->CR, ADC_CR_ADSTART, false);
}
