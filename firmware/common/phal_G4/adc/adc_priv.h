/**
 * @file adc_priv.h
 * @brief Private register-level API for the STM32G4 ADC PHAL.
 *
 * Used by adc.c to drive the ADC directly: clock setup, calibration, sequence
 * and sample-time programming, enable/disable, and conversion start/stop.
 * Not for direct use by application code.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#ifndef PHAL_G4_ADC_PRIV_H
#define PHAL_G4_ADC_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/phal_G4/adc/adc.h"

/// Return whether an ADC instance is supported by this driver.
bool ADC_PRIV_instance_is_supported(const ADC_TypeDef *instance);

/// Configure and calibrate a disabled ADC instance.
bool ADC_PRIV_configure(
    ADC_TypeDef *instance,
    const PHAL_ADC_ChannelConfig_t *channels,
    size_t channel_count
);

/// Enable an ADC instance and wait for it to become ready.
bool ADC_PRIV_enable(ADC_TypeDef *instance);

/// Stop and disable an ADC instance.
bool ADC_PRIV_disable(ADC_TypeDef *instance);

/// Return whether an ADC instance is enabled and ready.
bool ADC_PRIV_is_ready(const ADC_TypeDef *instance);

/// Clear transfer flags and select single or continuous DMA requests.
void ADC_PRIV_prepare_transfer(ADC_TypeDef *instance, bool continuous);

/// Start the configured regular conversion sequence.
void ADC_PRIV_start_conversion(ADC_TypeDef *instance);

/// Stop an active regular conversion sequence.
bool ADC_PRIV_stop_conversion(ADC_TypeDef *instance);

#endif // PHAL_G4_ADC_PRIV_H
