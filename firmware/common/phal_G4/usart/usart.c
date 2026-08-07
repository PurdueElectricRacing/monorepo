#include "common/phal_G4/usart/usart.h"
#include "common/phal_G4/usart/usart_priv.h"

#include "common/phal_G4/dma/dma.h"

typedef struct {
    PHAL_DMA_Handle_t tx_dma;    /*!< TX DMA handle (built in init) */
    PHAL_DMA_Handle_t rx_dma;    /*!< RX DMA handle (built in init) */
    volatile uint16_t rxfer_size; /*!< configured RX length (for continuous re-arm) */
    volatile uint16_t rx_len;    /*!< bytes actually received in the last completed frame */
    volatile bool tx_busy;       /*!< set when a TX is in flight, cleared by the USART TC ISR */
    volatile bool rx_busy;       /*!< set while a frame is in flight, cleared by the IDLE-line ISR */
    bool cont_rx;                /*!< continuous vs one-shot reception */
} PHAL_USART_state_t;

static PHAL_USART_state_t usart_state[NUM_USART];

/**
 * @brief Initialize a USART peripheral for DMA-driven communication.
 *
 * @param periph Which USART peripheral to initialize
 * @param baud_rate Desired baud rate
 * @param clock_rate Frequency (Hz) of the bus clock feeding this USART (APB1/APB2)
 * @return true on success, false if DMA init failed
 */
bool PHAL_USART_init(PHAL_USART_Idx_t periph, uint32_t baud_rate, const uint32_t clock_rate) {
    ssize_t idx = periph;

    PHAL_USART_priv_configure(idx, baud_rate, clock_rate);
    PHAL_USART_priv_buildDma(idx, &usart_state[idx].tx_dma, &usart_state[idx].rx_dma);

    // Both, not short-circuited: a failed TX claim must not leave the RX
    // handle uninitialized, since that failure mode is silent until the first
    // rx call returns false for no visible reason.
    bool tx_ready = PHAL_DMA_init(&usart_state[idx].tx_dma);
    bool rx_ready = PHAL_DMA_init(&usart_state[idx].rx_dma);

    return tx_ready && rx_ready;
}

/**
 * @brief Start a DMA-based transmission.
 *
 * @param periph Which USART peripheral to transmit on
 * @param data Buffer to send
 * @param len Number of bytes to send
 * @return true if every DMA reconfiguration step succeeded, false if a
 *         transmission is already in flight or a step failed
 */
bool PHAL_USART_tx(PHAL_USART_Idx_t periph, uint8_t *data, uint16_t len) {
    ssize_t idx = periph;

    if (usart_state[idx].tx_busy) {
        return false;
    }

    // Ensure flags are cleared & DMA configured
    PHAL_DMA_Handle_t *tx_dma = &usart_state[idx].tx_dma;
    bool stopped     = PHAL_DMA_stop(tx_dma);
    bool length_set  = PHAL_DMA_setLength(tx_dma, len);
    bool address_set = PHAL_DMA_setMemAddress(tx_dma, (uint32_t)data);
    bool restarted   = PHAL_DMA_restart(tx_dma);

    if (!(stopped && length_set && address_set && restarted)) {
        return false;
    }

    // After DMA enabled, mark channel as busy
    usart_state[idx].tx_busy = true;

    // Start transmission
    PHAL_USART_priv_startTx(PHAL_USART_priv_periph(idx));

    return true;
}

/**
 * @brief Start a DMA-based reception, completed on the IDLE line.
 *
 * @param periph Which USART peripheral to receive on
 * @param data Buffer to receive into
 * @param len Maximum number of bytes to receive (buffer size)
 * @param cont Enable continuous RX. When set, call this once and the HAL keeps
 *             receiving frames of the same maximum length, invoking
 *             PHAL_USART_rxCallback after each.
 * @return true if every DMA reconfiguration step succeeded, false otherwise
 */
bool PHAL_USART_rx(PHAL_USART_Idx_t periph, uint8_t *data, uint16_t len, bool cont) {
    ssize_t idx = periph;
    USART_TypeDef *hw = PHAL_USART_priv_periph(idx);

    // Quiesce the receiver before touching the channel. With RE and DMAR off,
    // no byte can be latched while we retarget it, which is what makes the
    // flush below safe from a byte arriving mid-sequence.
    PHAL_USART_priv_stopRx(hw);

    usart_state[idx].cont_rx = cont;
    usart_state[idx].rxfer_size = len;
    usart_state[idx].rx_len = 0;

    // Channel must be disabled to set address/length. Same reasoning as txDMA
    // above: run every step, then report whether they all actually succeeded.
    PHAL_DMA_Handle_t *rx_dma = &usart_state[idx].rx_dma;
    bool stopped     = PHAL_DMA_stop(rx_dma);
    bool address_set = PHAL_DMA_setMemAddress(rx_dma, (uint32_t)data);
    bool length_set  = PHAL_DMA_setLength(rx_dma, len);

    PHAL_USART_priv_flushRx(hw);

    bool restarted = PHAL_DMA_restart(rx_dma);

    if (!(stopped && address_set && length_set && restarted)) {
        return false;
    }

    usart_state[idx].rx_busy = true;
    PHAL_USART_priv_startRx(hw);

    return true;
}

/**
 * @brief Check whether a transmission is still in progress.
 *
 * @param periph Which USART peripheral to check
 * @return true if a transmission is in flight, false otherwise
 */
bool PHAL_USART_txBusy(PHAL_USART_Idx_t periph) {
    return usart_state[periph].tx_busy;
}

/**
 * @brief Number of bytes received in the last completed frame.
 *
 * @param periph Which USART peripheral to query
 * @return byte count, valid once PHAL_USART_rxCallback has fired
 */
uint16_t PHAL_USART_rxCount(PHAL_USART_Idx_t periph) {
    return usart_state[periph].rx_len;
}

/**
 * @brief Transmit data, blocking until the transfer completes.
 *
 * @param periph Which USART peripheral to transmit on
 * @param data Buffer to send
 * @param len Number of bytes to send
 * @return true if the transfer completed, false if it failed to start
 */
bool PHAL_USART_txBlocking(PHAL_USART_Idx_t periph, uint8_t *data, uint16_t len) {
    if (!PHAL_USART_tx(periph, data, len)) return false;

    while (PHAL_USART_txBusy(periph)) {
        __asm__("nop");
    }
    
    return true;
}

/**
 * @brief Receive data, blocking until a one-shot reception completes.
 *
 * @param periph Which USART peripheral to receive on
 * @param data Buffer to receive into
 * @param len Number of bytes to receive
 * @return true if the reception completed, false if it failed to start
 */
bool PHAL_USART_rxBlocking(PHAL_USART_Idx_t periph, uint8_t *data, uint16_t len) {
    if (!PHAL_USART_rx(periph, data, len, false)) return false;

    while (usart_state[periph].rx_busy) {
        __asm__("nop");
    }

    return true;
}

/// Service the USART's own interrupt: transmission complete and/or IDLE line.
static void PHAL_USART_HandleIRQ(PHAL_USART_Idx_t idx) {
    USART_TypeDef *periph = PHAL_USART_priv_periph(idx);

    // TX completion comes from the USART, not from the DMA. The DMA reports
    // done once its last write reaches TDR, with one byte still in TDR and
    // another mid-shift - up to two frame times before the wire goes quiet.
    // Releasing the caller there lets it overwrite the buffer or re-enable RE
    // while data is still going out.
    if (PHAL_USART_priv_txCompleteActive(periph)) {
        PHAL_USART_priv_finishTx(periph);
        usart_state[idx].tx_busy = false;
    }

    if (!PHAL_USART_priv_idleActive(periph)) {
        return;
    }

    // Clear IDLE up front. The re-arm below plus an application callback of
    // arbitrary length can easily outlast the gap to the next frame, and an
    // IDLE latched during that window has to survive this handler - clearing
    // at the end would discard it, leaving the channel un-rearmed so the next
    // frame lands at a stale offset.
    PHAL_USART_priv_clearIdle(periph);

    PHAL_DMA_Handle_t *rx_dma = &usart_state[idx].rx_dma;
    PHAL_DMA_stop(rx_dma);

    // CNDTR counts down, so the shortfall against the configured length is
    // what actually landed. Read it before the re-arm reloads the count.
    uint16_t received = (uint16_t)(usart_state[idx].rxfer_size - PHAL_DMA_getRemaining(rx_dma));
    usart_state[idx].rx_len = received;
    usart_state[idx].rx_busy = false;

    if (usart_state[idx].cont_rx) {
        // Same ordering rule as PHAL_USART_rx: receiver off, retarget, flush,
        // then live again. Leaving RE and DMAR on across the re-arm lets a
        // byte land in RDR while the channel is disabled (and a second one
        // raise ORE, which stalls the receiver entirely); that byte would then
        // be handed to data[0] of the next frame.
        PHAL_USART_priv_stopRx(periph);
        PHAL_DMA_setLength(rx_dma, usart_state[idx].rxfer_size);
        PHAL_USART_priv_flushRx(periph);
        // restart clears stale channel flags so a prior TEIF can't stall re-arm.
        PHAL_DMA_restart(rx_dma);
        usart_state[idx].rx_busy = true;
        PHAL_USART_priv_startRx(periph);
    } else {
        PHAL_USART_priv_stopRx(periph);
    }

    // Last, so the channel is already armed for the next frame and a slow
    // callback costs reception time rather than a whole frame.
    PHAL_USART_rxCallback(idx, received);
}

/// On TX DMA completion, release the channel. tx_busy is left alone - it is
/// cleared by the USART's transmission-complete interrupt, which is the point
/// the frame has actually finished going out.
static void PHAL_USART_HandleDMA(PHAL_USART_Idx_t idx) {
    if (PHAL_USART_priv_txDmaComplete(idx)) {
        PHAL_DMA_stop(&usart_state[idx].tx_dma);
    }
    PHAL_USART_priv_clearTxDmaFlags(idx);
}

[[gnu::weak]] void PHAL_USART_rxCallback(PHAL_USART_Idx_t periph, uint16_t len) {
    (void)periph;
    (void)len;
}

/* DMA transfer-complete interrupt handlers (TX channels) */
[[gnu::weak]]
void DMA1_Channel7_IRQHandler(void) {
    PHAL_USART_HandleDMA(USART1_IDX);
}

[[gnu::weak]]
void DMA1_Channel4_IRQHandler(void) {
    PHAL_USART_HandleDMA(USART2_IDX);
}

/// Service USART3 RX when it owns DMA1 channel 1.
void PHAL_USART_DMA1_Channel1_IRQHandler(void) {
    PHAL_USART_HandleDMA(USART3_IDX);
}

// ADC provides the strong shared vector when linked and delegates here when
// ADC1 does not own the channel. This weak vector covers USART-only builds.
[[gnu::weak]]
void DMA1_Channel1_IRQHandler(void) {
    PHAL_USART_DMA1_Channel1_IRQHandler();
}

/* USART Interrupt Handlers */
[[gnu::weak]]
void DMA1_Channel2_IRQHandler(void) {
    PHAL_USART_HandleDMA(USART3_IDX);
}

/* USART interrupt handlers (IDLE line + transmission complete) */
void USART1_IRQHandler(void) {
    PHAL_USART_HandleIRQ(USART1_IDX);
}

void USART2_IRQHandler(void) {
    PHAL_USART_HandleIRQ(USART2_IDX);
}

void USART3_IRQHandler(void) {
    PHAL_USART_HandleIRQ(USART3_IDX);
}
