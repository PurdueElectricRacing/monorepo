/**
 * @file telemetry.c
 * @brief DASHBOARD telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "telemetry.h"
#include "can_library/generated/DASHBOARD.h"
#include "common/bootloader/application_version.h"

// Callback to parse LWS data and forward to VCAN
void LWS_Standard_CALLBACK(void) {
    // forwards LWS data onto VCAN, simplifies flag parsing
    bool data_valid = can_data.LWS_Standard.OK && can_data.LWS_Standard.CAL && can_data.LWS_Standard.TRIM;

    CAN_SEND_steering_angle(
        can_data.LWS_Standard.LWS_ANGLE,
        can_data.LWS_Standard.LWS_SPEED,
        data_valid
    );
}

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: Dashboard version string
 */
static_assert(DASH_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_dash_version(GIT_HASH, APPLICATION_BOOTLOADABLE);
}