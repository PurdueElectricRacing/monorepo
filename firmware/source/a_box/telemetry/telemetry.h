/**
 * @file telemetry.h
 * @brief ABOX Telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

// todo maybe a better home for these
static inline float vbatt_to_voltage(uint16_t vbatt_raw) {
    // Example conversion:
    // 600V actual => linearly scaled to 2v -> linearly scaled to 4v -> (4.7/5.7) divider to ~3.3v
    // Full conversion:
    // V * (1/300) * 2 * (4.7/5.7) * 4095/3.3 = ADC
    // V = (300) * (1/2) * (5.7/4.7) * (3.3/4095) * ADC

    static constexpr float ADC_TO_PACK_VOLTAGE = 300.0f * (1.0f / 2.0f) * (5.7f / 4.7f) * (3.3f / 4095.0f);
    
    float voltage = vbatt_raw * ADC_TO_PACK_VOLTAGE;

    return voltage;
}

// DHAB S/134 current sensor conversion
static inline float isense_to_current(uint16_t isense_raw) {
    static constexpr float ADC_VREF = 3.3f;
    static constexpr float ADC_MAX  = 4095.0f;
    static constexpr float ADC_TO_VOLTS = ADC_VREF / ADC_MAX;

    static constexpr float DIV_R1   = 2400.0f;
    static constexpr float DIV_R2   = 4700.0f;
    static constexpr float DIV_GAIN = (DIV_R1 + DIV_R2) / DIV_R2;

    static constexpr float V_OFFSET = 2.5f;
    static constexpr float G        = 10.0e-3f;

    float v_adc      = isense_raw * ADC_TO_VOLTS;
    float v_sensor   = v_adc * DIV_GAIN;
    float current    = (v_sensor - V_OFFSET) / G; // data

    // Apply offset correction
    static constexpr float ISENSE_OFFSET_AMPS = 2.5f; // todo tune this
    current += ISENSE_OFFSET_AMPS;

    return current;
}

static constexpr uint32_t BMS_PERIOD_MS = 200;

static constexpr uint32_t TELEMETRY_100HZ_PERIOD_MS = 10;
void report_telemetry_100hz(void);

static constexpr uint32_t TELEMETRY_8HZ_PERIOD_MS = 125;
void report_telemetry_8hz(void);

static constexpr uint32_t TELEMETRY_02HZ_PERIOD_MS = 5000;
void report_telemetry_02hz(void);

#endif // TELEMETRY_H