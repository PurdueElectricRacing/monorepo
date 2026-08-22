/**
 * @file fdcan.c
 * @brief G4 FDCAN public API implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 * 
 * Logic level code. All register/message-RAM detail lives in fdcan_priv.c.
 */

#include "common/phal_G4/fdcan/fdcan.h"

#include "common/phal_G4/fdcan/fdcan_priv.h"


void PHAL_FDCAN_init(FDCAN_GlobalTypeDef *fdcan, PHAL_FDCAN_BaudRate_t bit_rate) {
    PHAL_FDCAN_priv_enableClock();

    PHAL_FDCAN_priv_enterConfig(fdcan);
 
    // Classic CAN config (no FD/BRS/loopback, auto-retransmission enabled, TX pause on)
    PHAL_FDCAN_priv_controlConfig(fdcan);
 
    // Time quantum nominal bit
    fdcan->NBTP = PHAL_FDCAN_priv_getNBTP(bit_rate);

    // FIFO mode for TX buffer
    PHAL_FDCAN_priv_setTransmitFifoQueueModeToFifo(fdcan);
 
    // RX new message -> line 0, TX complete -> line 1
    PHAL_FDCAN_setInterruptLines(fdcan);
 
    // Default filters: accept everything
    PHAL_FDCAN_priv_writeFilterAction(fdcan, FDCAN_PRIV_FILTER_ACCEPT_IN_RX_FIFO0);
 
    PHAL_FDCAN_priv_exitConfig(fdcan);
}

bool PHAL_FDCAN_setFilters(FDCAN_GlobalTypeDef *fdcan,
                           const uint32_t *sid_list,
                           uint32_t num_sid,
                           const uint32_t *xid_list,
                           uint32_t num_xid) {
    bool sid_exceeded = num_sid > PHAL_FDCAN_MAX_NUM_SID_FILTER;
    bool xid_exceeded = num_xid > PHAL_FDCAN_MAX_NUM_XID_FILTER;
    bool sid_missing  = (num_sid > 0U) && (sid_list == nullptr);
    bool xid_missing  = (num_xid > 0U) && (xid_list == nullptr);
     if ((fdcan == nullptr) || sid_exceeded || xid_exceeded || sid_missing || xid_missing) {
         return false;
     }
 
    PHAL_FDCAN_priv_enterConfig(fdcan);
 
    // Exact match: anything not in the list is rejected
    PHAL_FDCAN_priv_writeFilterAction(fdcan, FDCAN_PRIV_FILTER_REJECT);
 
    if (num_sid > 0) {
        PHAL_FDCAN_priv_writeStandardFilters(fdcan, sid_list, num_sid);
    }
    if (num_xid) {
        PHAL_FDCAN_priv_writeExtendedFilters(fdcan, xid_list, num_xid);
    }
 
    PHAL_FDCAN_priv_exitConfig(fdcan);
    return true;
}

bool PHAL_FDCAN_txFifoFree(const FDCAN_GlobalTypeDef *fdcan) {
    return !PHAL_FDCAN_priv_readTxFifoQueueStatusFullFlag(fdcan);
}
 
bool PHAL_FDCAN_send(CanMsgTypeDef_t *msg) {
    if (!PHAL_FDCAN_txFifoFree(msg->Bus)) {
        return false;
    }
 
    PHAL_FDCAN_priv_writeTxElement(msg->Bus, msg);
    return true;
}

[[gnu::always_inline]]
static inline void fdcan_rxIRQHandler(FDCAN_GlobalTypeDef *fdcan) {
    if (!PHAL_FDCAN_priv_readReceiveFifo0NewMessageInterruptFlag(fdcan)) {
        return;
    }
    PHAL_FDCAN_priv_clearReceiveFifo0NewMessageInterruptFlag(fdcan);

    CanMsgTypeDef_t msg;
    while (PHAL_FDCAN_priv_readRxElement(fdcan, &msg)) {
        PHAL_FDCAN_rxCallback(&msg);
    }
}

[[gnu::always_inline]]
static inline void fdcan_txIRQHandler(FDCAN_GlobalTypeDef *fdcan) {
    if (!PHAL_FDCAN_priv_readTransmitCompleteInterruptFlag(fdcan)) {
        return;
    }
    PHAL_FDCAN_priv_clearTransmitCompleteInterruptFlag(fdcan);

    PHAL_FDCAN_txCallback(fdcan);
}
 
[[gnu::weak]]
void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    (void)msg;
}
 
[[gnu::weak]]
void PHAL_FDCAN_txCallback(FDCAN_GlobalTypeDef *fdcan) {
    (void)fdcan;
}
 
void FDCAN1_IT0_IRQHandler(void) {
    fdcan_rxIRQHandler(FDCAN1);
}
 
void FDCAN1_IT1_IRQHandler(void) {
    fdcan_txIRQHandler(FDCAN1);
}
 
void FDCAN2_IT0_IRQHandler(void) {
    fdcan_rxIRQHandler(FDCAN2);
}
 
void FDCAN2_IT1_IRQHandler(void) {
    fdcan_txIRQHandler(FDCAN2);
}
 
void FDCAN3_IT0_IRQHandler(void) {
    fdcan_rxIRQHandler(FDCAN3);
}
 
void FDCAN3_IT1_IRQHandler(void) {
    fdcan_txIRQHandler(FDCAN3);
}
