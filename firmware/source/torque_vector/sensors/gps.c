/**
 * @file gps.c
 * @brief GPS RX and parsing thread
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/faults_common.h"
#include "common/rtos/rtos.h"
#include "sensors.h"

volatile uint8_t rover_rx_buffer[ROVER_TX_SIZE] = {0}; // updated by DMA stream
NAV_PVT_data_t nav_pvt = {0};
NAV_RELPOSNED_data_t nav_relposned = {0};

volatile uint32_t last_gps_message_time_ms = 0;
void gps_periodic(void) {
    // todo block until woken by dma interrupt

    last_gps_message_time_ms = xTaskGetTickCount();

    // decode raw data stream
    NAV_PVT_decode(&nav_pvt, rover_rx_buffer);
    NAV_RELPOSNED_decode(&nav_relposned, (rover_rx_buffer + NAV_PVT_TOTAL_LENGTH));

    // parse fix flags
    bool is_fix_ok = nav_pvt.flags1 & GPS_FLAG1_GNSS_FIX_OK;
    update_fault(FAULT_ID_GPS_INVALID_FIX, !is_fix_ok);

    bool is_strong_fix =
        (nav_pvt.fixType == GPS_FIX_TYPE_GNSS_2D) ||
        (nav_pvt.fixType == GPS_FIX_TYPE_GNSS_3D) ||
        (nav_pvt.fixType == GPS_FIX_TYPE_GNSS_DEAD_RECKONING); 
    update_fault(FAULT_ID_GPS_WEAK_FIX, !is_strong_fix);

    bool is_UTC_resolved = nav_pvt.valid & GPS_VALID_FULLY_RESOLVED;
    update_fault(FAULT_ID_GPS_INVALID_UTC, !is_UTC_resolved);
}