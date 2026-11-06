/**
 * @file vbatt_vmc.c
 * @brief MAIN MODULE vbatt and vmc reading implementations
 */

#include "vbatt_vmc.h"

#include "main.h"

// HV resistor divider, 0-600V -> 0-2V so the AMC3311 input is in range.
static constexpr float HV_DIV_RTOP_OHM   = 5.0f * 475000.0f; // R2,R4,R6,R7,R9: 5 x 475k in series
static constexpr float HV_DIV_RSENSE_OHM = 7870.0f + 73.2f;   // R14 + R15: bottom of divider

// AMC3311 isolated amplifier, isolates HV from LV side; unity gain per datasheet.
static constexpr float AMC3311_GAIN      = 1.0f;              // U2

// differential-to-single-ended op amp, 0-2V -> 0-4V.
static constexpr float DIFF_AMP_GAIN     = 2000.0f / 1000.0f; // U3: R13 (2k) / R10 (1k)

// divider after U1 buffer, brings 0-4V down into the MCU's 0-3.3V ADC range.
static constexpr float MCU_DIV_R3_OHM    = 2400.0f;           // R3: top resistor
static constexpr float MCU_DIV_R5_OHM    = 4700.0f;           // R5: bottom resistor

// Multiply the ADC pin voltage by this to get the real voltage (~226.6 V per V).
// Each stage is inverted: dividers multiply by (Rtop+Rbot)/Rbot, amps divide by gain.
static constexpr float HV_PIN_TO_SOURCE_GAIN =
    ((HV_DIV_RTOP_OHM + HV_DIV_RSENSE_OHM) / HV_DIV_RSENSE_OHM)
    / AMC3311_GAIN
    / DIFF_AMP_GAIN
    * ((MCU_DIV_R3_OHM + MCU_DIV_R5_OHM) / MCU_DIV_R5_OHM);

// VBATT and VMC use the same sense circuit, so they share the same gain.
static constexpr float VBATT_PIN_TO_SOURCE_GAIN = HV_PIN_TO_SOURCE_GAIN;
static constexpr float VMC_PIN_TO_SOURCE_GAIN   = HV_PIN_TO_SOURCE_GAIN;

// Converts raw ADC counts to the voltage seen at the MCU pin.
static float adc_counts_to_pin_volts(uint16_t raw) {
    static constexpr float ADC_VREF_V     = 3.3f;    // ADC reference = 3.3V supply
    static constexpr float ADC_FULL_SCALE = 4095.0f; // 12-bit ADC max count (2^12 - 1)

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
