/**
 * @file fdcan_priv.h
 * @brief G4 FDCAN private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PHAL_G4_FDCAN_PRIV_H
#define PHAL_G4_FDCAN_PRIV_H

#include <stdint.h>

#include "common/phal_G4/phal_G4.h"


/// The FDCAN kernel clock frequency (the clock NBTP's prescaler divides down.
/// Our HAL for FDCAN is always clocked from PCLK1.
/// NOTE: delicate to PCLK1 changes!!
static constexpr uint32_t FDCAN_PRIV_KER_CLK_HZ = 16'000'000U;

// Values originate from ST's HAL & documentation

/// FDCAN clock is derived from PCLK1
static constexpr uint32_t FDCAN_PRIV_RCC_FDCANCLKSOURCE_PCLK1 = RCC_CCIPR_FDCANSEL_1;

/// Filter action values for RXGFC register
/// Controls what happens to any frame that does not match any of the configured filters
typedef enum : uint32_t {
	FDCAN_PRIV_FILTER_ACCEPT_IN_RX_FIFO0 = 0x00000000U,
	FDCAN_PRIV_FILTER_REJECT             = 0x00000002U
} PHAL_FDCAN_DefaultFilterAction_t;

// Message RAM layout. Same layout, different base offset for each peripheral

static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLS_NBR = 28; /// Max standard id filters
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLE_NBR = 8;  /// Max extended id filters
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF0_NBR = 3;  /// RX FIFO 0 Elements Number
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF1_NBR = 3;  /// RX FIFO 1 Elements Number
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TEF_NBR = 3;  /// TX Event FIFO Elements Number
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TFQ_NBR = 3;  /// TX FIFO Elements Number

static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLS_SIZE = 1 * 4;  // Filter Standard Element Size in bytes
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLE_SIZE = 2 * 4;  // Filter Extended Element Size in bytes

/// Size in bytes of one RX FIFO 0 element (set by the hardware format)
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF0_SIZE = 18 * 4;

static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF1_SIZE = 18 * 4; // RX FIFO 1 Elements Size in bytes
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TEF_SIZE = 2 * 4;  // TX Event FIFO Elements Size in bytes

/// Size in bytes of one TX FIFO/queue element (set by the hardware format)
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TFQ_SIZE = 18 * 4;

/// Filter List Standard Start Address
/// - the byte offset, within this instance's Message RAM slice, where the
///   array of standard (11-bit ID) filter elements begins
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLSSA = 0;

/// Filter List Extended Start Address
/// - the byte offset, within this instance's Message RAM slice, where the
///   array of extended (29-bit ID) filter elements begins
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_FLESA = FDCAN_PRIV_SRAMCAN_FLSSA + FDCAN_PRIV_SRAMCAN_FLS_NBR * FDCAN_PRIV_SRAMCAN_FLS_SIZE;

/// Rx FIFO 0 Start Address
/// - byte offset where the RX FIFO 0 element array begins in this instance's Message RAM slice
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF0SA = FDCAN_PRIV_SRAMCAN_FLESA + FDCAN_PRIV_SRAMCAN_FLE_NBR * FDCAN_PRIV_SRAMCAN_FLE_SIZE;
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_RF1SA = FDCAN_PRIV_SRAMCAN_RF0SA + FDCAN_PRIV_SRAMCAN_RF0_NBR * FDCAN_PRIV_SRAMCAN_RF0_SIZE;
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TEFSA = FDCAN_PRIV_SRAMCAN_RF1SA + FDCAN_PRIV_SRAMCAN_RF1_NBR * FDCAN_PRIV_SRAMCAN_RF1_SIZE;

/// Tx FIFO/Queue Start Address
/// - byte offset where the TX FIFO/queue element array begins in this instance's Message RAM slice
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_TFQSA = FDCAN_PRIV_SRAMCAN_TEFSA + FDCAN_PRIV_SRAMCAN_TEF_NBR * FDCAN_PRIV_SRAMCAN_TEF_SIZE;

/// Bytes required/consumed by one instance's full Message RAM layout (filters + RX FIFOs + TX event FIFO + TX FIFO)
static constexpr uint32_t FDCAN_PRIV_SRAMCAN_SIZE  = FDCAN_PRIV_SRAMCAN_TFQSA + FDCAN_PRIV_SRAMCAN_TFQ_NBR * FDCAN_PRIV_SRAMCAN_TFQ_SIZE;


/// Base address of this peripheral's slice of shared message RAM
uint32_t PHAL_FDCAN_priv_ramBase(const FDCAN_GlobalTypeDef *fdcan);

/// Enable FDCAN kernel + peripheral clocks (PCLK1)
void PHAL_FDCAN_priv_enableClock(void);

/// Enter INIT mode and unlock configuration registers. Blocks until entered
void PHAL_FDCAN_priv_enterConfig(FDCAN_GlobalTypeDef *fdcan);

/// Leave INIT mode, ending configuration. Blocks until left
void PHAL_FDCAN_priv_exitConfig(FDCAN_GlobalTypeDef *fdcan);

// Configure CCCR and TEST registers for classic CAN operation
// Not FD, not BRS, not loopback, auto-retransmission enabled, TX pause on
void PHAL_FDCAN_priv_controlConfig(FDCAN_GlobalTypeDef *fdcan);

/// Set TX FIFO/Queue mode to FIFO (not queue)
void PHAL_FDCAN_priv_setTransmitFifoQueueModeToFifo(FDCAN_GlobalTypeDef *fdcan);

/// Set up RX new message -> line 0, TX complete -> line 1
void PHAL_FDCAN_setInterruptLines(FDCAN_GlobalTypeDef *fdcan);

/// Write a filter action to RXGFC (accept/reject all)
void PHAL_FDCAN_priv_writeFilterAction(FDCAN_GlobalTypeDef *fdcan, PHAL_FDCAN_DefaultFilterAction_t action);

/// Compute the NBTP register value for one of the supported bit rates.
/// 16-time-quantum nominal bit time (~87.5% sample point)
uint32_t PHAL_FDCAN_priv_getNBTP(PHAL_FDCAN_BaudRate_t bit_rate);

/// Set num_sid exact-match standard filters and forward to RX FIFO0
void PHAL_FDCAN_priv_writeStandardFilters(FDCAN_GlobalTypeDef *fdcan, const uint32_t *sid_list, uint32_t num_sid);

/// Set num_xid exact-match extended filters and forward to RX FIFO0 
void PHAL_FDCAN_priv_writeExtendedFilters(FDCAN_GlobalTypeDef *fdcan, const uint32_t *xid_list, uint32_t num_xid);

/// Write one classic-CAN TX element for msg into the next free TX FIFO slot
/// Not safe to call if the TX FIFO is full
void PHAL_FDCAN_priv_writeTxElement(FDCAN_GlobalTypeDef *fdcan, CanMsgTypeDef_t *msg);

/// Pop and decode the oldest element from RX FIFO0
// return true and fills msg on success. False if RX FIFO0 is empty
bool PHAL_FDCAN_priv_readRxElement(FDCAN_GlobalTypeDef *fdcan, CanMsgTypeDef_t *msg);

/// Check whether the TX FIFO/Queue is full
bool PHAL_FDCAN_priv_readTxFifoQueueStatusFullFlag(const FDCAN_GlobalTypeDef *fdcan);

/// Check whether the RX FIFO0 new message interrupt flag is set
bool PHAL_FDCAN_priv_readReceiveFifo0NewMessageInterruptFlag(const FDCAN_GlobalTypeDef *fdcan);

/// Clear the RX FIFO0 new message interrupt flag
void PHAL_FDCAN_priv_clearReceiveFifo0NewMessageInterruptFlag(FDCAN_GlobalTypeDef *fdcan);

/// Check whether the TX complete interrupt flag is set
bool PHAL_FDCAN_priv_readTransmitCompleteInterruptFlag(const FDCAN_GlobalTypeDef *fdcan);

/// Clear the TX complete interrupt flag
void PHAL_FDCAN_priv_clearTransmitCompleteInterruptFlag(FDCAN_GlobalTypeDef *fdcan);

#endif // PHAL_G4_FDCAN_PRIV_H
