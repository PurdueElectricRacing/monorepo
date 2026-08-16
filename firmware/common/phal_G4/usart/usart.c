#include "common/phal_G4/usart/usart.h"
#include "common/phal_G4/usart/usart_priv.h"

#include "common/phal_G4/dma/dma.h"

// Forward declaration of the DMA callback handler for TX completion
static void usart_tx_dma_callback(void *ctx);

typedef struct {
    PHAL_DMA_Handle_t tx_dma;     /*!< TX DMA handle (built in init) */
    PHAL_DMA_Handle_t rx_dma;     /*!< RX DMA handle (built in init) */
    volatile uint16_t rxfer_size; /*!< configured RX length (for continuous re-arm) */
    volatile uint16_t rx_len;     /*!< bytes actually received in the last completed frame */
    volatile bool tx_busy;        /*!< set when a TX is in flight, cleared by the USART TC ISR */
    volatile bool rx_busy;        /*!< set while a frame is in flight, cleared by the IDLE-line ISR */
    bool cont_rx;                 /*!< continuous vs one-shot reception */
} PHAL_USART_state_t;

static PHAL_USART_state_t usart_state[NUM_USART];

/**
 * @brief Initialize a USART peripheral for DMA-driven communication.
 *
 * @param periph_idx Which USART peripheral to initialize
 * @param baud_rate Desired baud rate
 * @param clock_rate Frequency (Hz) of the bus clock feeding this USART (APB1/APB2)
 * @return true on success, false if DMA init failed
 */
bool PHAL_USART_init(PHAL_USART_Idx_t periph_idx, uint32_t baud_rate, const uint32_t clock_rate) {
    PHAL_USART_priv_configure(periph_idx, baud_rate, clock_rate);
    PHAL_USART_priv_buildDma(periph_idx, &usart_state[periph_idx].tx_dma, &usart_state[periph_idx].rx_dma);

    // Both, not short-circuited: a failed TX claim must not leave the RX
    // handle uninitialized, since that failure mode is silent until the first
    // rx call returns false for no visible reason.
    // TX completion -> PHAL_USART_HandleDMA
    // RX completion comes from the USART IDLE-line interrupt
    bool tx_ready = PHAL_DMA_initWithCallback(&usart_state[periph_idx].tx_dma, usart_tx_dma_callback, (void *)(intptr_t)periph_idx);
    bool rx_ready = PHAL_DMA_init(&usart_state[periph_idx].rx_dma);

    if (tx_ready && rx_ready) {
        PHAL_USART_priv_enableIrq(periph_idx);
    }

    return tx_ready && rx_ready;
}

/**
 * @brief Start a DMA-based transmission.
 *
 * @param periph_idx Which USART peripheral to transmit on
 * @param data Buffer to send
 * @param len Number of bytes to send
 * @return true if every DMA reconfiguration step succeeded, false if a
 *         transmission is already in flight or a step failed
 */
bool PHAL_USART_tx(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len) {
    if (usart_state[periph_idx].tx_busy) {
        return false;
    }

    PHAL_DMA_Handle_t *tx_dma = &usart_state[periph_idx].tx_dma;
    PHAL_DMA_stop(tx_dma);
    if (!PHAL_DMA_setLength(tx_dma, len) || !PHAL_DMA_setMemAddress(tx_dma, (uint32_t)data)) {
        return false;
    }
    PHAL_DMA_restart(tx_dma);

    usart_state[periph_idx].tx_busy = true;

    PHAL_USART_priv_startTx(PHAL_USART_priv_periph(periph_idx));

    return true;
}

/**
 * @brief Start a DMA-based reception, completed on the IDLE line.
 *
 * @param periph_idx Which USART peripheral to receive on
 * @param data Buffer to receive into
 * @param len Maximum number of bytes to receive (buffer size)
 * @param cont Enable continuous RX. When set, call this once and the HAL keeps
 *             receiving frames of the same maximum length, invoking
 *             PHAL_USART_rxCallback after each.
 * @return true if every DMA reconfiguration step succeeded, false otherwise
 */
bool PHAL_USART_rx(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len, bool cont) {
    USART_TypeDef *periph = PHAL_USART_priv_periph(periph_idx);

    PHAL_USART_priv_stopRx(periph);

    usart_state[periph_idx].cont_rx = cont;
    usart_state[periph_idx].rxfer_size = len;
    usart_state[periph_idx].rx_len = 0;

    PHAL_DMA_Handle_t *rx_dma = &usart_state[periph_idx].rx_dma;
    PHAL_DMA_stop(rx_dma);
    if (!PHAL_DMA_setMemAddress(rx_dma, (uint32_t)data) || !PHAL_DMA_setLength(rx_dma, len)) {
        return false;
    }

    PHAL_USART_priv_flushRx(periph);

    PHAL_DMA_restart(rx_dma);

    usart_state[periph_idx].rx_busy = true;
    PHAL_USART_priv_startRx(periph);

    return true;
}

/**
 * @brief Check whether a transmission is still in progress.
 *
 * @param periph_idx Which USART peripheral to check
 * @return true if a transmission is in flight, false otherwise
 */
bool PHAL_USART_txBusy(PHAL_USART_Idx_t periph_idx) {
    return usart_state[periph_idx].tx_busy;
}

/**
 * @brief Number of bytes received in the last completed frame.
 *
 * @param periph_idx Which USART peripheral to query
 * @return byte count, valid once PHAL_USART_rxCallback has fired
 */
uint16_t PHAL_USART_rxCount(PHAL_USART_Idx_t periph_idx) {
    return usart_state[periph_idx].rx_len;
}

/**
 * @brief Transmit data, blocking until the transfer completes.
 *
 * @param periph_idx Which USART peripheral to transmit on
 * @param data Buffer to send
 * @param len Number of bytes to send
 * @return true if the transfer completed, false if it failed to start
 */
bool PHAL_USART_txBlocking(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len) {
    if (!PHAL_USART_tx(periph_idx, data, len)) return false;

    while (PHAL_USART_txBusy(periph_idx)) {
        __asm__("nop");
    }
    
    return true;
}

/**
 * @brief Receive data, blocking until a one-shot reception completes.
 *
 * @param periph_idx Which USART peripheral to receive on
 * @param data Buffer to receive into
 * @param len Number of bytes to receive
 * @return true if the reception completed, false if it failed to start
 */
bool PHAL_USART_rxBlocking(PHAL_USART_Idx_t periph_idx, uint8_t *data, uint16_t len) {
    if (!PHAL_USART_rx(periph_idx, data, len, false)) return false;

    while (usart_state[periph_idx].rx_busy) {
        __asm__("nop");
    }

    return true;
}

static void usart_handle_irq(PHAL_USART_Idx_t periph_idx) {
    USART_TypeDef *periph = PHAL_USART_priv_periph(periph_idx);

    if (PHAL_USART_priv_txCompleteActive(periph)) {
        PHAL_USART_priv_finishTx(periph);
        usart_state[periph_idx].tx_busy = false;
    }

    if (!PHAL_USART_priv_idleActive(periph)) {
        return;
    }

    PHAL_USART_priv_clearIdle(periph);

    PHAL_DMA_Handle_t *rx_dma = &usart_state[periph_idx].rx_dma;

    // Enabling RE on an already-idle line can raise IDLE before the first byte reaches DMA
    if (PHAL_DMA_getRemaining(rx_dma) == usart_state[periph_idx].rxfer_size) {
        return;
    }

    PHAL_DMA_stop(rx_dma);

    // CNDTR counts down, so the shortfall against the configured length is
    // what actually landed. Read it before the re-arm reloads the count.
    uint16_t received = (uint16_t)(usart_state[periph_idx].rxfer_size - PHAL_DMA_getRemaining(rx_dma));
    usart_state[periph_idx].rx_len = received;
    usart_state[periph_idx].rx_busy = false;

    if (usart_state[periph_idx].cont_rx) {
        PHAL_USART_priv_stopRx(periph);
        PHAL_DMA_setLength(rx_dma, usart_state[periph_idx].rxfer_size);
        PHAL_USART_priv_flushRx(periph);
        PHAL_DMA_restart(rx_dma);

        usart_state[periph_idx].rx_busy = true;
        PHAL_USART_priv_startRx(periph);
    } else {
        PHAL_USART_priv_stopRx(periph);
    }

    PHAL_USART_rxCallback(periph_idx, received);
}

static void usart_handle_dma(PHAL_USART_Idx_t periph_idx) {
    PHAL_DMA_Handle_t *tx_dma = &usart_state[periph_idx].tx_dma;
    if (PHAL_DMA_isComplete(tx_dma)) {
        PHAL_DMA_stop(&usart_state[periph_idx].tx_dma);
    }
    PHAL_DMA_clearFlags(tx_dma);
}

static void usart_tx_dma_callback(void *ctx) {
    usart_handle_dma((PHAL_USART_Idx_t)(intptr_t)ctx);
}

[[gnu::weak]] void PHAL_USART_rxCallback(PHAL_USART_Idx_t periph_idx, uint16_t len) {
    (void)periph_idx;
    (void)len;
}

/* USART interrupt handlers (IDLE line + transmission complete) */
void USART1_IRQHandler(void) {
    usart_handle_irq(USART1_IDX);
}

void USART2_IRQHandler(void) {
    usart_handle_irq(USART2_IDX);
}

void USART3_IRQHandler(void) {
    usart_handle_irq(USART3_IDX);
}