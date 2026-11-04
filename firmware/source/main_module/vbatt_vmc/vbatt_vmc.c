/**
 * @file vbatt_vmc.c
 * @brief MAIN MODULE vbatt and vmc reading implementations
 */

#include "vbatt_vmc.h"

#include "main.h"

static constexpr float VBATT_PIN_TO_SOURCE_GAIN = 1.0f;
static constexpr float VMC_PIN_TO_SOURCE_GAIN   = 1.0f;

static float adc_counts_to_pin_volts(uint16_t raw) {
    static constexpr float ADC_VREF_V     = 3.3f;
    static constexpr float ADC_FULL_SCALE = 4095.0f;

    return (float)raw * (ADC_VREF_V / ADC_FULL_SCALE);
}

float vbatt_get_voltage(void) {
    uint16_t raw = raw_adc2_values.vbatt;
    return adc_counts_to_pin_volts(raw) * VBATT_PIN_TO_SOURCE_GAIN;
}

float vmc_get_voltage(void) {
    uint16_t raw = raw_adc1_values.vmc;
    return adc_counts_to_pin_volts(raw) * VMC_PIN_TO_SOURCE_GAIN;
}
