#include <string.h>

#include "common/phal_G4/fdcan/fdcan.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "main.h"

volatile uint32_t last_can_rx_time_ms = 0;

// Callback must be ISR safe
void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    timestamped_frame_t frame = {0};

    frame.ticks_ms      = xTaskGetTickCountFromISR();
    last_can_rx_time_ms = frame.ticks_ms;

    set_bus_id(&frame, 0); // todo check msg->Bus
    set_xid(&frame, msg->IDE);
    set_can_id(&frame, msg->ExtId);

    memcpy(&frame.payload, msg->Data, msg->DLC);

    xQueueSendToBackFromISR(can_queue, &frame, 0);
}