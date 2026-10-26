/**
 * @file telemetry.c
 * @brief DASHBOARD telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "telemetry.h"

#include "can_library/generated/DASHBOARD.h"
#include "can_library/generated/can_version.h"
#include "common/bootloader/bootloader_common.h"
#include "common/utils/clamp.h"
#include "common/utils/rescale.h"
#include "main.h"

static inline float brake_adc_to_pressure(uint16_t raw_brake_adc) {
    static constexpr float BRAKE_PRESSURE_ADC_MIN = 414.0f;  // ADC count at 0 PSI (sensor 0.5V -> divider 0.333V)
    static constexpr float BRAKE_PRESSURE_ADC_MAX = 3723.0f; // ADC count at max-rated PSI (sensor 4.5V -> divider 3.0V)
    static_assert(BRAKE_PRESSURE_ADC_MIN < BRAKE_PRESSURE_ADC_MAX, "Invalid brake pressure ADC calibration values");

    static constexpr float BRAKE_PRESSURE_PSI_MIN = 0.0f;
    static constexpr float BRAKE_PRESSURE_PSI_MAX = 3771.0f; // 260 bar * 14.5038 psi/bar
    static_assert(BRAKE_PRESSURE_PSI_MIN < BRAKE_PRESSURE_PSI_MAX, "Invalid brake pressure PSI calibration values");

    float clamped_adc = CLAMP(raw_brake_adc, BRAKE_PRESSURE_ADC_MIN, BRAKE_PRESSURE_ADC_MAX);
    return RESCALE(clamped_adc, BRAKE_PRESSURE_ADC_MIN, BRAKE_PRESSURE_ADC_MAX,
                   BRAKE_PRESSURE_PSI_MIN, BRAKE_PRESSURE_PSI_MAX);
}

/**
 * @brief Reports telemetry data at 25 Hz rate
 * Includes: Brake pressure
 */
static_assert(BRAKE_PRESSURE_PERIOD_MS == TELEMETRY_25HZ_PERIOD_MS);
void report_telemetry_25hz(void) {
    // Harness maps brake pressure to regen1.
    uint16_t brake_pressure = (uint16_t)(brake_adc_to_pressure(raw_adc_values.regen1) * PACK_COEFF_BRAKE_PRESSURE_BRAKE_PRESSURE);

    CAN_SEND_brake_pressure(brake_pressure);
}

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: Dashboard version string
 */
static_assert(DASH_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_dash_version(FIRMWARE_GIT_HASH, BL_getGitHash(), APPLICATION_BOOTLOADABLE);
}
