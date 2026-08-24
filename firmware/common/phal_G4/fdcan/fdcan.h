/**
 * @file fdcan.h
 * @brief G4 FDCAN public API implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PHAL_G4_FDCAN_H
#define PHAL_G4_FDCAN_H

#include <stdint.h>

#include "stm32g474xx.h"

static constexpr uint32_t PHAL_FDCAN_MAX_NUM_SID_FILTER = 28;
static constexpr uint32_t PHAL_FDCAN_MAX_NUM_XID_FILTER = 8;

/**
 * @brief Classic CAN frame
 * 
 * used for both TX and RX.
 */
typedef struct {
    FDCAN_GlobalTypeDef* Bus; /*!< When RX - Bus = which peripheral the frame arrived on
                                   When TX - Bus =  which peripheral should transmits it */
    bool IDE;                 /*!< true = extended ID, false = standard ID */
    union {
        uint16_t StdId; /*!< valid when !IDE, 11-bit */
        uint32_t ExtId; /*!< valid when IDE,  29-bit */
    };
    uint8_t DLC;     /*!< payload length, 0-8 */
    uint8_t Data[8]; /*!< payload bytes */
} CanMsgTypeDef_t;

/**
 * @brief Supported baud rates for PER G4 FDCAN HAL.
 */
typedef enum : uint32_t {
    FDCAN_BAUD_250K = 250000U,
    FDCAN_BAUD_500K = 500000U,
    FDCAN_BAUD_1M   = 1000000U
} PHAL_FDCAN_BaudRate_t;


/**
 * @brief Initialize an FDCAN peripheral for classic (non-FD) CAN operation
 *
 * - Setups up FDCAN clock (from PCLK1)
 * - ~87.5% sample point
 * - Classic CAN mode (FD/BRS off), auto-retransmission enabled, TX pause on
 * - TX FIFO mode
 * - RX FIFO0 new message (line 0) and TX complete interrupts (line 1)
 * - Sets filter to accept everything into RX FIFO0
 *    - Use PHAL_FDCAN_setFilters later
 *
 * @param fdcan Peripheral instance (FDCAN1/2/3)
 * @param bit_rate desired bit rate in bits per second
 */
void PHAL_FDCAN_init(FDCAN_GlobalTypeDef *fdcan, PHAL_FDCAN_BaudRate_t bit_rate);

/**
 * @brief Configure exact-match acceptance filters for RX FIFO0
 *
 * Every ID in sid_list/xid_list is matched exactly.
 * Any message whose ID is not in either list is rejected.
 *
 * Note: this replaces any previously existing filter configurations
 *
 * @param fdcan Peripheral instance (FDCAN1/2/3)
 * @param sid_list array of standard (11-bit) IDs to accept; may be NULL if num_sid == 0
 * @param num_sid number of entries in sid_list, up to MAX_NUM_SID_FILTER allowed
 * @param xid_list array of extended (29-bit) IDs to accept; may be NULL if num_xid == 0
 * @param num_xid number of entries in xid_list, up to MAX_NUM_XID_FILTER allowed
 * @return true on success; false if num_sid or num_xid exceeds its max
 */
bool PHAL_FDCAN_setFilters(
    FDCAN_GlobalTypeDef *fdcan,
    const uint32_t *sid_list,
    uint32_t num_sid,
    const uint32_t *xid_list,
    uint32_t num_xid
);

/**
 * @brief Queue a frame for transmission
 *
 * Non-blocking: if the TX FIFO is already full this returns false
 * instead of blocking/waiting.
 *
 * @param msg frame to send; sent on msg->Bus peripheral
 * @return true if the frame was queued; false if the TX FIFO is full
 */
bool PHAL_FDCAN_send(CanMsgTypeDef_t *msg);

/**
 * @brief Check whether the TX FIFO has at least one free slot.
 * @param fdcan peripheral instance
 * @return true if a slot is free
 */
bool PHAL_FDCAN_txFifoFree(const FDCAN_GlobalTypeDef *fdcan);

/**
 * @brief Weak callback fired once per received frame
 *
 * Called from FDCANx_IT0_IRQHandler (interrupt context) for every frame
 * popped from RX FIFO0.
 *
 * Default implementation does nothing.
 *
 * @param msg the received frame (valid only for the duration of the call)
 */
extern void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg);

/**
 * @brief Weak callback fired when a queued frame finishes transmitting
 *
 * Called from FDCANx_IT1_IRQHandler (interrupt context).
 * 
 * Default implementation does nothing.
 *
 * @param fdcan the peripheral instance that completed a transmission
 */
extern void PHAL_FDCAN_txCallback(FDCAN_GlobalTypeDef *fdcan);


#endif // PHAL_G4_FDCAN_H
