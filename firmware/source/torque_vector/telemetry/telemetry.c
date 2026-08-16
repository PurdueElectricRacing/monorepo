/**
 * @file telemetry.c
 * @brief TORQUE VECTOR telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "telemetry.h"

#include "can_library/faults_common.h"
#include "can_library/generated/TORQUE_VECTOR.h"
#include "common/bootloader/application_version.h"
#include "common/utils/clamp.h"
#include "sensors.h"

/**
 * @brief Reports telemetry data at 25HZ rate
 * Includes: GPS coordinates, velocity, speed, heading
 */
static_assert(GPS_COORDINATES_PERIOD_MS == TELEMETRY_25HZ_PERIOD_MS);
static_assert(GPS_VELOCITY_PERIOD_MS == TELEMETRY_25HZ_PERIOD_MS);
static_assert(GPS_SPEED_HEADING_PERIOD_MS == TELEMETRY_25HZ_PERIOD_MS);
void report_telemetry_25hz(void) {
    if (is_clear(FAULT_ID_GPS_INVALID_FIX) && is_clear(FAULT_ID_GPS_WEAK_FIX)) {
        CAN_SEND_gps_coordinates(nav_pvt.latitude, nav_pvt.longitude);
        CAN_SEND_gps_velocity(nav_pvt.velNorth, nav_pvt.velEast);
        CAN_SEND_gps_speed_heading(nav_pvt.groundSpeed, nav_pvt.headingVehicle);
    }
}

/**
 * @brief Reports the application hash and bootloader capability at 0.2 Hz.
 */
static_assert(TORQUE_VECTOR_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_torque_vector_version(GIT_HASH, APPLICATION_BOOTLOADABLE);
}

/**
 * @brief Reports telemetry data at 1HZ rate
 * Includes: GPS time (UTC)
 */
static_assert(GPS_TIME_PERIOD_MS == TELEMETRY_1HZ_PERIOD_MS);
void report_telemetry_1hz(void) {
    if (is_clear(FAULT_ID_GPS_INVALID_UTC)) {
        uint16_t milliseconds = (uint16_t)CLAMP(nav_pvt.nano / 1'000'000, 0, 999);

        CAN_SEND_gps_time(
            (uint8_t)(nav_pvt.year - 2000),
            nav_pvt.month,
            nav_pvt.day,
            nav_pvt.hour,
            nav_pvt.minute,
            nav_pvt.second,
            milliseconds
        );
    }
}