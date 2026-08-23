#ifndef __PHAL_G4_USART_H__
#define __PHAL_G4_USART_H__

#include "stm32g474xx.h"

typedef enum {
    USART1_IDX = 0,
    USART2_IDX = 1,
    USART3_IDX = 2,
    NUM_USART
} PHAL_USART_Idx_t;

/**
 * @brief Initialize a USART peripheral for DMA-driven communication.
 *
 * Enables the peripheral clock and applies a fixed frame format:
 *   - 8 data bits, no parity, 1 stop bit (8N1)
 *   - 16x oversampling
 *   - no hardware flow control
 * Derives the baud-rate divisor from clock_rate, enables the IDLE-line
 * interrupt (RX frame completion) and the TX DMA transfer-complete interrupt,
 * then initializes the TX and RX DMA channels.
 *
 * Call once per USART before any tx/rx. The USART GPIO pins must already be
 * configured by the caller.
 *
 * @param periph_idx Which USART peripheral to initialize
 * @param baud_rate Desired baud rate
 * @param clock_rate Frequency (Hz) of the bus clock feeding this USART (APBx)
 * @return true on success, false if DMA init failed
 */
bool PHAL_USART_init(PHAL_USART_Idx_t periph_idx, uint32_t baud_rate, const uint32_t clock_rate);

/**
 * @brief Start a transmission using DMA.
 *
 * Fails rather than truncating a transfer already in flight - poll
 * PHAL_USART_txBusy() (or use PHAL_USART_txBlocking) before calling again.
 *
 * @param periph_idx Which USART peripheral to transmit on
 * @param data The address of the data to send
 * @param len Number of bytes.
 * @return true if every DMA reconfiguration step succeeded, false if a
 *         transmission is already in flight or a step failed
 */
bool PHAL_USART_tx(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len);

/**
 * @brief Start a reception using DMA of a specific length.
 *
 * @param periph_idx Which USART peripheral to receive on
 * @param data The address to put the received data
 * @param len Maximum number of bytes (the buffer size). A frame shorter than
 *            this still completes on the IDLE line; the actual count is
 *            reported to PHAL_USART_rxCallback. uint16_t because that is the
 *            DMA's transfer counter width.
 * @param cont Enable continuous RX using the IDLE-line interrupt. When set, call
 *             this function once and the HAL keeps receiving frames of the same
 *             maximum length, invoking PHAL_USART_rxCallback after each.
 */
void PHAL_USART_rx(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len, bool cont);

/**
 * @brief Returns whether the USART peripheral is currently transmitting data.
 *
 * Tracks the USART's own transmission-complete flag, so it stays true until
 * the last bit has left the wire - not merely until the DMA has finished
 * writing to the data register.
 *
 * @param periph_idx Which USART peripheral to check
 * @return true if the peripheral is currently sending a message, false otherwise
 */
bool PHAL_USART_txBusy(PHAL_USART_Idx_t periph_idx);

/**
 * @brief Number of bytes received in the last completed frame.
 *
 * The IDLE line ends a frame whenever the sender stops, which may be short of
 * the length passed to PHAL_USART_rx. Anything past this count in the buffer
 * is leftover from an earlier frame.
 *
 * @param periph_idx Which USART peripheral to query
 * @return byte count, valid once PHAL_USART_rxCallback has fired (or
 *         PHAL_USART_rxBlocking has returned)
 */
uint16_t PHAL_USART_rxCount(PHAL_USART_Idx_t periph_idx);

/**
 * @brief Transmit data, blocking until the transfer completes.
 *
 * Starts a DMA transmission (see PHAL_USART_tx) and busy-waits until the
 * USART reports the frame fully shifted out. Do not call from an ISR.
 *
 * @param periph_idx Which USART peripheral to transmit on
 * @param data Buffer to send
 * @param len Number of bytes to send
 * @return true if the transfer completed, false if it failed to start
 */
bool PHAL_USART_txBlocking(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len);

/**
 * @brief Receive data, blocking until a one-shot reception completes.
 *
 * Starts a one-shot DMA reception (see PHAL_USART_rx) and busy-waits until
 * the IDLE-line ISR signals the frame is complete. Call PHAL_USART_rxCount()
 * afterwards for the byte count. Do not call from an ISR.
 *
 * @param periph_idx Which USART peripheral to receive on
 * @param data Buffer to receive into
 * @param len Maximum number of bytes to receive
 */
void PHAL_USART_rxBlocking(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len);

/**
 * @brief Weak callback invoked when a full RX frame is received. Override in
 *        application code. Runs in ISR context, so keep it light.
 *
 * @param periph_idx Which USART peripheral received the frame
 * @param len Number of bytes in this frame. Bytes past this offset in the
 *            buffer are stale and must not be decoded.
 */
extern void PHAL_USART_rxCallback(PHAL_USART_Idx_t periph_idx, uint16_t len);

#endif // __PHAL_G4_USART_H__
