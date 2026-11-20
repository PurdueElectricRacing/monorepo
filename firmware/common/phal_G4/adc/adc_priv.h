/**
 * @file adc_priv.h
 * @brief G4 ADC register-level helpers, used only by adc.c.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#ifndef PHAL_G4_ADC_PRIV_H
#define PHAL_G4_ADC_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"

/// Clock, calibrate, and program the sequence; leaves the ADC disabled
bool ADC_PRIV_configure(
    ADC_TypeDef *instance,
    const uint8_t *channels,
    size_t channel_count
);

/// Enable and wait for ADRDY
bool ADC_PRIV_enable(ADC_TypeDef *instance);

/// Stop conversions and disable
bool ADC_PRIV_disable(ADC_TypeDef *instance);

/// Clear flags; select continuous or one-shot DMA requests
void ADC_PRIV_prepare_transfer(ADC_TypeDef *instance, bool continuous);

/// Set ADSTART
void ADC_PRIV_start_conversion(ADC_TypeDef *instance);

/// Set ADSTP and wait for ADSTART to clear
bool ADC_PRIV_stop_conversion(ADC_TypeDef *instance);

#endif // PHAL_G4_ADC_PRIV_H
