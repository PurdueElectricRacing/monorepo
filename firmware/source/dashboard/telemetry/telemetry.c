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

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: Dashboard version string
 */
static_assert(DASH_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_dash_version(FIRMWARE_GIT_HASH, BL_getGitHash(), APPLICATION_BOOTLOADABLE);
}