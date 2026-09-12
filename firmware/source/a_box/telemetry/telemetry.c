/**
 * @file telemetry.c
 * @brief ABOX Telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "telemetry.h"

#include "can_library/generated/A_BOX.h"
#include "common/bootloader/application_version.h"
#include "common/bootloader/bootloader_common.h"
#include "main.h"

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

/**
 * @brief Reports telemetry data at 100 Hz rate
 * Includes: Pack analog stats and cell telemetry
 */
static_assert(PACK_ANALOG_PERIOD_MS == TELEMETRY_100HZ_PERIOD_MS);
static_assert(PACK_ANALOG_CCAN_PERIOD_MS == TELEMETRY_100HZ_PERIOD_MS);
static_assert(CELL_TELEMETRY_PERIOD_MS == TELEMETRY_100HZ_PERIOD_MS);
static_assert(CELL_TELEMETRY_CCAN_PERIOD_MS == TELEMETRY_100HZ_PERIOD_MS);
void report_telemetry_100hz(void) {
    uint16_t pack_voltage = (uint16_t)(vbatt_to_voltage(adc1_dma_buffer.vbatt_raw) * PACK_COEFF_PACK_ANALOG_PACK_VOLTAGE);
    int16_t pack_current  = (int16_t)(isense_to_current(adc1_dma_buffer.isense_raw) * PACK_COEFF_PACK_ANALOG_PACK_CURRENT);

    CAN_SEND_pack_analog(pack_voltage, pack_current);
    CAN_SEND_pack_analog_ccan(pack_voltage, pack_current);

    // Report cell voltages one at a time
    static uint8_t module_num      = 0;
    static uint8_t cell_num        = 0;
    adbms_module_t *current_module = &g_bms.modules[module_num];

    float cell_voltage = current_module->cell_voltages[cell_num];
    uint16_t scaled_cell_voltage = (uint16_t)(cell_voltage * PACK_COEFF_CELL_TELEMETRY_VOLTAGE);
    bool is_balancing  = current_module->is_discharging[cell_num];

    CAN_SEND_cell_telemetry(scaled_cell_voltage, module_num, cell_num, is_balancing);
    CAN_SEND_cell_telemetry_ccan(scaled_cell_voltage, module_num, cell_num, is_balancing);

    cell_num++;
    if (cell_num >= ADBMS6380_CELL_COUNT) {
        cell_num = 0;
        module_num++;
        if (module_num >= ADBMS_MODULE_COUNT) {
            module_num = 0;
        }
    }
}

/**
 * @brief Reports telemetry data at 8 Hz rate
 * Includes: Thermal stats
 */
static_assert(THERMISTOR_TELEMETRY_PERIOD_MS == TELEMETRY_8HZ_PERIOD_MS);
static_assert(THERMISTOR_TELEMETRY_CCAN_PERIOD_MS == TELEMETRY_8HZ_PERIOD_MS);
void report_telemetry_8hz(void) {
    // Report thermistor temperatures one at a time
    static uint8_t module_num      = 0;
    static uint8_t thermistor_num  = 0;
    adbms_module_t *current_module = &g_bms.modules[module_num];

    float thermistor_temperature = current_module->therms_temps[thermistor_num];
    uint16_t scaled_temperature = (uint16_t)(thermistor_temperature * PACK_COEFF_THERMISTOR_TELEMETRY_TEMPERATURE);

    CAN_SEND_thermistor_telemetry(scaled_temperature, module_num, thermistor_num);
    CAN_SEND_thermistor_telemetry_ccan(scaled_temperature, module_num, thermistor_num);

    if (++thermistor_num >= ADBMS6380_GPIO_COUNT) {
        thermistor_num = 0;
        if (++module_num >= ADBMS_MODULE_COUNT) {
            module_num = 0;
        }
    }
}

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: ABOX git hash
 */
static_assert(ABOX_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_abox_version(GIT_HASH, BL_getGitHash(), APPLICATION_BOOTLOADABLE);
}