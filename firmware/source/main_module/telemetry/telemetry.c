/**
 * @file telemetry.c
 * @brief MAIN MODULE telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "telemetry.h"

#include "can_library/generated/MAIN_MODULE.h"
#include "common/bootloader/application_version.h"
#include "powertrain.h"

/**
 * @brief Reports telemetry data at 50 Hz rate
 * Includes: AMK reported wheel_speeds
 */
static_assert(WHEEL_SPEEDS_PERIOD_MS == TELEMETRY_50HZ_PERIOD_MS);
void report_telemetry_50hz(void) {
    CAN_SEND_wheel_speeds(
        g_powertrain.front_right.crit->AMK_ActualSpeed,
        g_powertrain.front_left.crit->AMK_ActualSpeed,
        g_powertrain.rear_left.crit->AMK_ActualSpeed,
        g_powertrain.rear_right.crit->AMK_ActualSpeed
    );
}

static inline bool any_amk_message_stale(AMK_t *amk) {
    return 
        amk->crit->is_stale()
        || amk->err1->is_stale()
        || amk->err2->is_stale()
        || amk->info->is_stale()
        || amk->temps->is_stale();
}

/**
 * @brief Reports telemetry data at 1 Hz rate
 * Includes: AMK diagnostics
 */
static_assert(INVA_DIAGNOSTICS_PERIOD_MS == TELEMETRY_1HZ_PERIOD_MS);
static_assert(INVB_DIAGNOSTICS_PERIOD_MS == TELEMETRY_1HZ_PERIOD_MS);
static_assert(INVC_DIAGNOSTICS_PERIOD_MS == TELEMETRY_1HZ_PERIOD_MS);
static_assert(INVD_DIAGNOSTICS_PERIOD_MS == TELEMETRY_1HZ_PERIOD_MS);
void report_telemetry_1hz(void) {
    AMK_t *amks[] = {
        &g_powertrain.front_right,
        &g_powertrain.front_left,
        &g_powertrain.rear_left,
        &g_powertrain.rear_right
    };

    CAN_SEND_inva_diagnostics(
        amks[0]->err1->AMK_DiagnosticNumber,
        amks[0]->state,
        amks[0]->info->AMK_Status_bError,
        amks[0]->info->AMK_Status_bInverterOn,
        any_amk_message_stale(amks[0])
    );

    CAN_SEND_invb_diagnostics(
        amks[1]->err1->AMK_DiagnosticNumber,
        amks[1]->state,
        amks[1]->info->AMK_Status_bError,
        amks[1]->info->AMK_Status_bInverterOn,
        any_amk_message_stale(amks[1])
    );

    CAN_SEND_invc_diagnostics(
        amks[2]->err1->AMK_DiagnosticNumber,
        amks[2]->state,
        amks[2]->info->AMK_Status_bError,
        amks[2]->info->AMK_Status_bInverterOn,
        any_amk_message_stale(amks[2])
    );

    CAN_SEND_invd_diagnostics(
        amks[3]->err1->AMK_DiagnosticNumber,
        amks[3]->state,
        amks[3]->info->AMK_Status_bError,
        amks[3]->info->AMK_Status_bInverterOn,
        any_amk_message_stale(amks[3])
    );
}

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: AMK reported motor and IGBT temperatures, firmware version
 */
static_assert(MOTOR_TEMPS_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
static_assert(IGBT_TEMPS_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
static_assert(MAIN_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_motor_temps(
        g_powertrain.front_right.temps->AMK_MotorTemp,
        g_powertrain.front_left.temps->AMK_MotorTemp,
        g_powertrain.rear_left.temps->AMK_MotorTemp,
        g_powertrain.rear_right.temps->AMK_MotorTemp
    );

    CAN_SEND_igbt_temps(
        g_powertrain.front_right.temps->AMK_IGBTTemp,
        g_powertrain.front_left.temps->AMK_IGBTTemp,
        g_powertrain.rear_left.temps->AMK_IGBTTemp,
        g_powertrain.rear_right.temps->AMK_IGBTTemp
    );

    CAN_SEND_main_version(GIT_HASH, APPLICATION_BOOTLOADABLE);
}
