/**
 * @file telemetry.c
 * @brief PDU telemetry task implementations
 */

#include "telemetry.h"

#include "can_library/generated/PDU.h"
#include "can_library/generated/can_version.h"
#include "common/bootloader/bootloader_common.h"

/**
 * @brief Reports telemetry data at 0.2 Hz rate
 * Includes: PDU git hash, CAN stats
 */
static_assert(PDU_VERSION_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
static_assert(PDU_CAN_OVERFLOW_PERIOD_MS == TELEMETRY_02HZ_PERIOD_MS);
void report_telemetry_02hz(void) {
    CAN_SEND_pdu_version(FIRMWARE_GIT_HASH, BL_getGitHash(), APPLICATION_BOOTLOADABLE);
    CAN_SEND_pdu_can_overflow(can_stats.rx_overflow, can_stats.tx_overflow);
}
