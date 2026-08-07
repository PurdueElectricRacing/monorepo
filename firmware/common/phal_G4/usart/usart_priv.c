#include "common/phal_G4/usart/usart_priv.h"

static const PHAL_USART_HwMap_t USART_MAP[NUM_USART] = {
    [USART1_IDX] = {
        .rcc_enable_rg  = &RCC->APB2ENR,
        .rcc_enable_msk = RCC_APB2ENR_USART1EN,
        .rcc_reset_rg   = &RCC->APB2RSTR,
        .rcc_reset_msk  = RCC_APB2RSTR_USART1RST,
        .periph         = USART1,
        .irq            = USART1_IRQn,
        .tx_dma_irq     = DMA1_Channel7_IRQn,
        .tx_wiring      = &USART1_TX_DMA_WIRING,
        .rx_wiring      = &USART1_RX_DMA_WIRING,
    },
    [USART2_IDX] = {
        .rcc_enable_rg  = &RCC->APB1ENR1,
        .rcc_enable_msk = RCC_APB1ENR1_USART2EN,
        .rcc_reset_rg   = &RCC->APB1RSTR1,
        .rcc_reset_msk  = RCC_APB1RSTR1_USART2RST,
        .periph         = USART2,
        .irq            = USART2_IRQn,
        .tx_dma_irq     = DMA1_Channel4_IRQn,
        .tx_wiring      = &USART2_TX_DMA_WIRING,
        .rx_wiring      = &USART2_RX_DMA_WIRING,
    },
    [USART3_IDX] = {
        .rcc_enable_rg  = &RCC->APB1ENR1,
        .rcc_enable_msk = RCC_APB1ENR1_USART3EN,
        .rcc_reset_rg   = &RCC->APB1RSTR1,
        .rcc_reset_msk  = RCC_APB1RSTR1_USART3RST,
        .periph         = USART3,
        .irq            = USART3_IRQn,
        .tx_dma_irq     = DMA1_Channel2_IRQn,
        .tx_wiring      = &USART3_TX_DMA_WIRING,
        .rx_wiring      = &USART3_RX_DMA_WIRING,
    },
};

// Every RX error flag, plus IDLE. ICR is write-1-to-clear, so clearing a flag
// that is not set is harmless (RM0440, USART_ICR).
static constexpr uint32_t USART_PRIV_RX_FLAG_CLEAR_MSK =
      USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_NECF
    | USART_ICR_FECF | USART_ICR_PECF;

static inline uint32_t enterCritical(void) {
    uint32_t previous_interrupt_mask = __get_PRIMASK();
    __disable_irq();
    return previous_interrupt_mask;
}

static inline void exitCritical(uint32_t previous_interrupt_mask) {
    __set_PRIMASK(previous_interrupt_mask);
}

USART_TypeDef *PHAL_USART_priv_periph(ssize_t idx) {
    return USART_MAP[idx].periph;
}

void PHAL_USART_priv_configure(ssize_t idx, uint32_t baud_rate, uint32_t clock_rate) {
    const PHAL_USART_HwMap_t *map = &USART_MAP[idx];
    USART_TypeDef *periph = map->periph;

    // Pulse the peripheral reset
    *map->rcc_reset_rg |= map->rcc_reset_msk;
    *map->rcc_reset_rg &= ~map->rcc_reset_msk;

    // Enable the register clock
    *map->rcc_enable_rg |= map->rcc_enable_msk;

    // Dummy read to ensure the register write has taken effect
    (void)*map->rcc_enable_rg;

    // Reset control registers. We want:
    // 8 data bits, no parity, 1 stop bit,
    // 16x oversampling, disabled peripheral
    periph->CR1 = 0U;
    periph->CR2 = 0U;
    periph->CR3 = 0U;

    // Per original source code
    periph->BRR = (clock_rate + (baud_rate / 2U)) / baud_rate;

    // IDLE-line interrupt signals RX frame completion. TCIE stays off until a
    // transmission is actually armed, so an idle transmitter cannot raise the
    // shared USART interrupt.
    periph->CR1 |= USART_CR1_IDLEIE;

    // Enable USART
    periph->CR1 |= USART_CR1_UE;

    // ISR comes out of reset with TC already set; drop it (and any RX flags)
    // so the first startTx does not see a completion that never happened.
    periph->ICR = USART_PRIV_RX_FLAG_CLEAR_MSK | USART_ICR_TCCF;

    NVIC_ClearPendingIRQ(map->irq);
    NVIC_EnableIRQ(map->irq);

    NVIC_ClearPendingIRQ(map->tx_dma_irq);
    NVIC_EnableIRQ(map->tx_dma_irq);
}

void PHAL_USART_priv_buildDma(ssize_t idx, PHAL_DMA_Handle_t *tx_dma, PHAL_DMA_Handle_t *rx_dma) {
    const PHAL_USART_HwMap_t *map = &USART_MAP[idx];

    *tx_dma = (PHAL_DMA_Handle_t) {
        .wiring = map->tx_wiring,
        .params = {
            .priority  = DMA_PRIORITY_MEDIUM,
            .mode      = DMA_MODE_NORMAL,
            .mem_inc   = true,
            .tx_isr_en = true,
        },
    };

    *rx_dma = (PHAL_DMA_Handle_t) {
        .wiring = map->rx_wiring,
        .params = {
            .priority = DMA_PRIORITY_HIGH,
            .mode     = DMA_MODE_NORMAL,
            .mem_inc  = true,
            .tx_isr_en = false,
        },
    };
}

void PHAL_USART_priv_startTx(USART_TypeDef *periph) {
    uint32_t mask = enterCritical();

    // Drop the previous frame's TC before arming TCIE, otherwise enabling the
    // interrupt would immediately re-report a completion that already happened.
    periph->ICR = USART_ICR_TCCF;
    periph->CR1 |= USART_CR1_TE | USART_CR1_TCIE;

    // Last: the channel is already enabled, so this is the point the first
    // byte can actually move.
    periph->CR3 |= USART_CR3_DMAT;

    exitCritical(mask);
}

bool PHAL_USART_priv_txCompleteActive(USART_TypeDef *periph) {
    return (periph->ISR & USART_ISR_TC) != 0U && (periph->CR1 & USART_CR1_TCIE) != 0U;
}

void PHAL_USART_priv_finishTx(USART_TypeDef *periph) {
    uint32_t mask = enterCritical();

    periph->CR1 &= ~USART_CR1_TCIE;
    periph->ICR = USART_ICR_TCCF;

    exitCritical(mask);
}

void PHAL_USART_priv_startRx(USART_TypeDef *periph) {
    uint32_t mask = enterCritical();

    periph->CR1 |= USART_CR1_RE;
    periph->CR3 |= USART_CR3_DMAR;

    exitCritical(mask);
}

void PHAL_USART_priv_stopRx(USART_TypeDef *periph) {
    uint32_t mask = enterCritical();

    periph->CR1 &= ~USART_CR1_RE;
    periph->CR3 &= ~USART_CR3_DMAR;

    exitCritical(mask);
}

void PHAL_USART_priv_flushRx(USART_TypeDef *periph) {
    // RQR is write-only; RXFRQ discards whatever is in RDR and clears RXNE so
    // no stale byte is waiting when the DMA channel is enabled.
    periph->RQR = USART_RQR_RXFRQ;

    periph->ICR = USART_PRIV_RX_FLAG_CLEAR_MSK;
}

bool PHAL_USART_priv_idleActive(USART_TypeDef *periph) {
    return (periph->ISR & USART_ISR_IDLE) != 0U;
}

void PHAL_USART_priv_clearIdle(USART_TypeDef *periph) {
    periph->ICR = USART_ICR_IDLECF;
}

bool PHAL_USART_priv_txDmaComplete(ssize_t idx) {
    const PHAL_DMA_Wiring_t *wiring = USART_MAP[idx].tx_wiring;
    uint32_t shift = 4U * (wiring->channel_idx - 1U);
    return (wiring->periph->ISR & (DMA_ISR_TCIF1 << shift)) != 0U;
}

void PHAL_USART_priv_clearTxDmaFlags(ssize_t idx) {
    const PHAL_DMA_Wiring_t *wiring = USART_MAP[idx].tx_wiring;
    uint32_t shift = 4U * (wiring->channel_idx - 1U);
    // CGIF clears TCIF/HTIF/TEIF/GIF for the channel (RM0440, DMA_IFCR).
    wiring->periph->IFCR = DMA_IFCR_CGIF1 << shift;
}
