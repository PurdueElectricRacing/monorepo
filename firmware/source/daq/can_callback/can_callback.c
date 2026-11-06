/**
 * @file can_callback.c
 * @brief Timestamped frame producer from the CAN RX callback
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include <string.h>

#include "can_library/generated/MCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/phal_F4/can/can.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "rtc_sync.h"
#include "spmc.h"

volatile uint32_t last_can_rx_time_ms = 0;

static inline uint32_t get_bus_id(CAN_TypeDef* peripheral) {
    if (peripheral == CAN1) {
        return SCAN_BUS_ID;
    }
    if (peripheral == CAN2) {
        return VCAN_BUS_ID;
    }
    return MCAN_BUS_ID; // CAN3
}

void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg) {
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    timestamped_frame_t rx = {0};
    rx.ticks_ms            = xTaskGetTickCountFromISR();
    last_can_rx_time_ms    = rx.ticks_ms;

    const uint32_t bus_id = get_bus_id(msg->Bus);
    set_bus_id(&rx, bus_id);
    set_can_id(&rx, msg->IDE ? msg->ExtId : msg->StdId);

    memcpy(&rx.payload, msg->Data, msg->DLC);

    (void)SPMC_enqueue_from_ISR(&g_spmc, &rx);

    if (msg->Bus == CAN2 && !msg->IDE && msg->StdId == GPS_TIME_MSG_ID) {
        xQueueOverwriteFromISR(gps_time_queue, &rx, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
