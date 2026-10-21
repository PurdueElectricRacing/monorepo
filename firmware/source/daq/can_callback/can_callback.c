/**
 * @file can_callback.c
 * @brief Timestamped frame producer from the CAN RX callback
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include <string.h>

#include "can_library/generated/MCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/phal_F4/can/can.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "rtc_sync.h"
#include "spmc.h"

volatile uint32_t last_can_rx_time_ms = 0;

void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg) {
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    timestamped_frame_t rx = {0};
    rx.ticks_ms            = xTaskGetTickCountFromISR();
    last_can_rx_time_ms    = rx.ticks_ms;

    // CAN1 = VCAN, CAN2 = MCAN
    set_bus_id(&rx, (msg->Bus == CAN1) ? VCAN_BUS_ID : MCAN_BUS_ID);
    set_xid(&rx, msg->IDE);
    set_can_id(&rx, msg->IDE ? msg->ExtId : msg->StdId);

    memcpy(&rx.payload, msg->Data, msg->DLC);

    (void)SPMC_enqueue_from_ISR(&g_spmc, &rx);

    if (msg->Bus == CAN1 && !msg->IDE && msg->StdId == GPS_TIME_MSG_ID) {
        xQueueOverwriteFromISR(gps_time_queue, &rx, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
