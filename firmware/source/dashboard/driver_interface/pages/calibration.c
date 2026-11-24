/**
 * @file calibration.c
 * @brief Calibration page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "calibration.h"

#include "nextion/nextion.h"
#include "fault_library/faults_common.h"
#include "main.h"
#include "colors.h"

/**
 * @brief Updates the LCD display with current pedal telemetry data when on CALIBRATION page
 *
 * Updates brake and throttle bars, raw ADC values, deviation percentages, and status
 * indicators for brake and throttle pedals. Also displays fault statuses if detected.
 *
 * @note Only executes when current page is PAGE_CALIBRATION
 */
void calibration_telemetry_update() {
    if (is_latched(FAULT_ID_APPS_WIRING_T1)) {
        NXT_setText(CALIBRATION_STATUS, "T1 WIRING FAULT");
        NXT_setFontColor(CALIBRATION_STATUS, RED);
    } else if (is_latched(FAULT_ID_APPS_WIRING_T2)) {
        NXT_setText(CALIBRATION_STATUS, "T2 WIRING FAULT");
        NXT_setFontColor(CALIBRATION_STATUS, RED);
    } else if (is_latched(FAULT_ID_BSE)) {
        NXT_setText(CALIBRATION_STATUS, "BSE");
        NXT_setFontColor(CALIBRATION_STATUS, RED);
    } else if (is_latched(FAULT_ID_APPS_IMPLAUSIBLE)) {
        NXT_setText(CALIBRATION_STATUS, "IMPLAUSIBLE");
        NXT_setFontColor(CALIBRATION_STATUS, RED);
    } else if (is_latched(FAULT_ID_APPS_BRAKE)){
        NXT_setText(CALIBRATION_STATUS, "BRAKE-THROTTLE");
        NXT_setFontColor(CALIBRATION_STATUS, RED);
    } else {
        NXT_setText(CALIBRATION_STATUS, "OK");
        NXT_setFontColor(CALIBRATION_STATUS, GREEN);
    }

    NXT_setTextFormatted(CALIBRATION_THROTTLE1, "%d", raw_adc_values.throttle1);
    NXT_setTextFormatted(CALIBRATION_THROTTLE2, "%d", 4095 - raw_adc_values.throttle2);
    NXT_setTextFormatted(CALIBRATION_BRAKE1, "%d", raw_adc_values.regen1);
    NXT_setTextFormatted(CALIBRATION_BRAKE2, "%d", raw_adc_values.regen2);
    NXT_setTextFormatted(CALIBRATION_BRAKE_PRS1, "%d", raw_adc_values.brake1_pressure);
    NXT_setTextFormatted(CALIBRATION_BRAKE_PRS2, "%d", raw_adc_values.brake2_pressure);
}