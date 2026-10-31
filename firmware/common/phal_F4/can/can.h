/**
 * @file can.h
 * @brief F4 bxCAN public API
 *
 * @author Adam Busch (busch8@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef _PHAL_CAN_H
#define _PHAL_CAN_H

#include "common/phal_F4/phal_F4.h"

// Bit timing recovered from http://www.bittiming.can-wiki.info/

#define PHAL_CAN_16MHZ_1M (0x033a0000)
#define PHAL_CAN_24MHZ_1M (0x03270001)
#define PHAL_CAN_36MHZ_1M (0x033c0001)
#define PHAL_CAN_42MHZ_1M (0x034e0001)

#define PHAL_CAN_16MHZ_500K (0x033a0001) // sample point = 75%, SJW = 4
#define PHAL_CAN_24MHZ_500K (0x033a0002) // sample point = 75%, SJW = 4
#define PHAL_CAN_36MHZ_500K (0x03270005)
#define PHAL_CAN_42MHZ_500K (0x034e0003) // sample point = 75%, SJW = 4

#define PHAL_CAN_16MHZ_250K (0x003a0003) // sample point = 75%
#define PHAL_CAN_24MHZ_250K (0x003a0005) // sample point = 75%
#define PHAL_CAN_36MHZ_250K (0x003a0008) // sample point = 75%

#define PHAL_CAN_MAX_NUM_SID_FILTER (28U)
#define PHAL_CAN_MAX_NUM_XID_FILTER (8U)

/**
 * @brief Supported baud rates for PER F4 CAN HAL.
 */
typedef enum : uint32_t {
    FDCAN_BAUD_250K = 250'000U,
    FDCAN_BAUD_500K = 500'000U,
    FDCAN_BAUD_1M   = 1'000'000U
} PHAL_FDCAN_BaudRate_t;

/**
 * @brief Classic CAN frame
 * 
 * used for both TX and RX.
 */
typedef struct {
    CAN_TypeDef *Bus; /*!< When RX - Bus = which peripheral the frame arrived on
                           When TX - Bus = which peripheral should transmits it */
    bool IDE;

    union {
        uint16_t StdId; /*!< valid when !IDE, 11-bit */
        uint32_t ExtId; /*!< valid when IDE,  29-bit */
    };

    uint8_t DLC;     /*!< payload length, 0-8 */
    uint8_t Data[8]; /*!< payload bytes */
} CanMsgTypeDef_t;

/**
 * Initialize the bus with an accept-all FIFO0 filter, waiting for hardware acknowledgement.
 * Bus must be CAN1, CAN2 or CAN3 (nullptr is rejected).
 * Return false for an invalid bus or unsupported clock/baud combination, before changing hardware.
 */
bool PHAL_CAN_init(CAN_TypeDef *bus, PHAL_FDCAN_BaudRate_t bit_rate);

/** Replace this bus's filters. Empty lists accept all IDs; invalid input returns false. */
bool PHAL_CAN_setFilters(CAN_TypeDef *bus,
                         const uint32_t *sid_list,
                         uint32_t num_sid,
                         const uint32_t *xid_list,
                         uint32_t num_xid);

/** Queue a frame in any free mailbox; return false immediately if none is free. */
bool PHAL_CAN_send(CanMsgTypeDef_t *msg);

/** Return true if a mailbox is free; false for an invalid bus (including nullptr). */
bool PHAL_CAN_txFifoFree(const CAN_TypeDef *bus);

/* Override these weak callbacks in application code. RX messages live only during the callback. */

extern void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg);
extern void PHAL_CAN_txCallback(CAN_TypeDef *can);

#endif // _PHAL_CAN_H
