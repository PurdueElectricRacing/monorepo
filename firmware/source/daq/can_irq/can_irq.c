/**
 * @file can_irq.c
 * @brief Timestamped frame producers from CAN RX IRQ
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_irq.h"

#include "can_library/generated/VCAN.h"
#include "common/rtos/rtos.h"
#include "spmc.h"
#include "timestamped_frame.h"
#include "rtc_sync.h"

volatile uint32_t last_can_rx_time_ms = 0;

[[gnu::always_inline]]
static inline void can_rx_irq_handler(CAN_TypeDef *peripheral) {
    portBASE_TYPE xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    // message !empty
    bool message_pending = (peripheral->RF0R & CAN_RF0R_FMP0_Msk);
    if (!message_pending) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }

    timestamped_frame_t rx = {0}; // temp stack allocated variable
    rx.ticks_ms            = xTaskGetTickCountFromISR();
    last_can_rx_time_ms    = rx.ticks_ms;

    // CAN1 = VCAN = bus 0
    set_bus_id(&rx, (peripheral == CAN1) ? 0 : 1);

    CAN_FIFOMailBox_TypeDef *mailbox = &peripheral->sFIFOMailBox[0];
    bool is_xid = (mailbox->RIR & CAN_RI0R_IDE) != 0;
    set_xid(&rx, is_xid);

    // the right shift logic makes sense when you read RM0090 pg 1122
    uint32_t can_id;
    if (is_xid) {
        can_id = (mailbox->RIR & (CAN_RI0R_STID | CAN_RI0R_EXID)) >> CAN_RI0R_EXID_Pos;
    } else {
        can_id = (mailbox->RIR & CAN_RI0R_STID) >> CAN_RI0R_STID_Pos;
    }
    set_can_id(&rx, can_id);

    // copy payload
    rx.payload |= (uint64_t)(mailbox->RDLR);
    rx.payload |= (uint64_t)(mailbox->RDHR) << 32;

    (void)SPMC_enqueue_from_ISR(&g_spmc, &rx);

    // release the FIFO
    peripheral->RF0R |= CAN_RF0R_RFOM0;

    if (peripheral == CAN1 && can_id == GPS_TIME_MSG_ID) {
        xQueueOverwriteFromISR(gps_time_queue, &rx, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return;
}

void CAN1_RX0_IRQHandler() {
    can_rx_irq_handler(CAN1);
}

void CAN2_RX0_IRQHandler() {
    can_rx_irq_handler(CAN2);
}