#ifndef DAQ_CAN_BUSES_H
#define DAQ_CAN_BUSES_H

#include "can_library/generated/MCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"

#define DAQ_SCAN_CAN     CAN1
#define DAQ_SCAN_RX0_IRQ CAN1_RX0_IRQn
#define DAQ_VCAN_CAN     CAN2
#define DAQ_VCAN_RX0_IRQ CAN2_RX0_IRQn
#define DAQ_MCAN_CAN     CAN3
#define DAQ_MCAN_RX0_IRQ CAN3_RX0_IRQn

// Called by the PHAL RX callback, which supplies one of the three valid controllers.
static inline uint32_t can_bus_id(const CAN_TypeDef* peripheral) {
    if (peripheral == DAQ_SCAN_CAN) {
        return SCAN_BUS_ID;
    }
    if (peripheral == DAQ_VCAN_CAN) {
        return VCAN_BUS_ID;
    }
    return MCAN_BUS_ID; // DAQ_MCAN_CAN
}

#endif
