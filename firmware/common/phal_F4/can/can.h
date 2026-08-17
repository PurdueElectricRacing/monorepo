/**
 * @file can.h
 * @author Adam Busch (busch8@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2021-03-20
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef _PHAL_CAN_H
#define _PHAL_CAN_H

#include "common/phal_F4/phal_F4.h"

#define PHAL_CAN_TX_TIMEOUT   (5000U)
#define PHAL_CAN_INIT_TIMEOUT (5000U)

// Bit timing recovered from http://www.bittiming.can-wiki.info/

#define PHAL_CAN_16MHz_1M (0x033a0000) 
#define PHAL_CAN_24MHz_1M (0x03270001) 
#define PHAL_CAN_36MHz_1M (0x033c0001)
#define PHAL_CAN_42MHz_1M (0x034e0001) 

#define PHAL_CAN_16MHz_500k (0x033a0001) // sample point = 75%, SJW = 4
#define PHAL_CAN_24MHz_500k (0x033a0002) // sample point = 75%, SJW = 4
#define PHAL_CAN_36MHz_500k (0x03270005)
#define PHAL_CAN_42MHz_500k (0x034e0003) // sample point = 75%, SJW = 4

#define PHAL_CAN_16MHz_250k (0x003a0003) // sample point = 75%
#define PHAL_CAN_24MHz_250k (0x003a0005) // sample point = 75%
#define PHAL_CAN_36MHz_250k (0x003a0008) // sample point = 75%
/**
 * @brief Supported baud rates for PER F4 FDCAN HAL.
 */
typedef enum : uint32_t {
    FDCAN_BAUD_250K = 250000U,
    FDCAN_BAUD_500K = 500000U,
    FDCAN_BAUD_1M   = 1000000U
} PHAL_FDCAN_BaudRate_t;

/**
 * @brief Classic CAN frame
 * 
 * used for both TX and RX.
 */
typedef struct {
    CAN_TypeDef* Bus; /*!< When RX - Bus = which peripheral the frame arrived on
                                   When TX - Bus =  which peripheral should transmits it */
    bool IDE;
    union {
        uint16_t StdId; /*!< valid when !IDE, 11-bit */
        uint32_t ExtId; /*!< valid when IDE,  29-bit */
    };
    uint8_t DLC;        /*!< payload length, 0-8 */
    uint8_t Data[8];    /*!< payload bytes */
} CanMsgTypeDef_t;

/**
 * @brief Initilize CAN peripheral to bit_rate.
 *
 * @param test_mode Initilize CAN peripheral for self test mode
 * @param bit_rate  Bit rate in bps (i.e. 500000)
 *
 * @return true Peripheral sucessfully initalized
 * @return false Peripheral stalled during initilization
 */
bool PHAL_initCAN(CAN_TypeDef* bus, bool test_mode, uint32_t bit_rate);

bool PHAL_deinitCAN(CAN_TypeDef* bus);

/**
 * @brief Find an empty TX mailbox and transmit a CAN message if one is found.
 * Function will block until sucessful transmission of message until a specified timeout.
 *
 * @param can CAN peripheral to transmit with
 * @param msgId Message ID
 * @return true Sucessful TX of message.
 * @return false Unable to find empty message or transmit took too long.
 */
bool PHAL_txCANMessage(CanMsgTypeDef_t* msg, uint8_t mbx);
bool PHAL_txMailboxFree(CAN_TypeDef* bus, uint8_t mbx);
void PHAL_txCANAbort(CAN_TypeDef* bus, uint8_t mbx);
bool PHAL_rxCANMessage(CAN_TypeDef *bus, uint8_t fifo, CanMsgTypeDef_t *msg);
bool PHAL_anyTxMailboxFree(CAN_TypeDef* bus);
bool PHAL_getFreeTxMailbox(CAN_TypeDef* bus, uint8_t* mbx);
extern void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg);
extern void PHAL_CAN_txCallback(CAN_TypeDef *can);

#endif // _PHAL_CAN_H