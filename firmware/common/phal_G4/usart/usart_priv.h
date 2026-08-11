#ifndef __PHAL_G4_USART_PRIV_H__
#define __PHAL_G4_USART_PRIV_H__

#include <sys/types.h> // ssize_t

#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/phal_G4.h"
#include "common/phal_G4/usart/usart.h" // PHAL_USART_Idx_t

typedef struct {
    volatile uint32_t *rcc_enable_rg;   /*!< RCC enable register for this UART */
    uint32_t rcc_enable_msk;            /*!< enable bit within rcc_enable_rg */
    volatile uint32_t *rcc_reset_rg;    /*!< RCC reset register for this UART */
    uint32_t rcc_reset_msk;             /*!< reset bit within rcc_reset_rg */
    USART_TypeDef *periph;              /*!< peripheral instance */
    IRQn_Type irq;                      /*!< USART global interrupt (carries IDLE and TC) */
    IRQn_Type tx_dma_irq;               /*!< NVIC line for the TX DMA channel */
    const PHAL_DMA_Wiring_t *tx_wiring; /*!< fixed DMA wiring for TX (see dma_wiring.h) */
    const PHAL_DMA_Wiring_t *rx_wiring; /*!< fixed DMA wiring for RX (see dma_wiring.h) */
} PHAL_USART_HwMap_t;

/// @return the peripheral instance for a slot (used by the interrupt handlers).
USART_TypeDef *PHAL_USART_priv_periph(ssize_t idx);

/// Enable the clock, program 8N1 + baud, and enable the USART + TX-DMA interrupts.
void PHAL_USART_priv_configure(ssize_t idx, uint32_t baud_rate, uint32_t clock_rate);

/// Fill the TX and RX DMA handles from the hardware map.
void PHAL_USART_priv_buildDma(ssize_t idx, PHAL_DMA_Handle_t *tx_dma, PHAL_DMA_Handle_t *rx_dma);

void PHAL_USART_priv_startTx(USART_TypeDef *periph);

/// Acknowledge transmission-complete: disable TCIE and clear ISR.TC.
void PHAL_USART_priv_finishTx(USART_TypeDef *periph);

bool PHAL_USART_priv_txCompleteActive(USART_TypeDef *periph);

/// Enable the receiver and its DMA request line (CR1.RE, CR3.DMAR).
void PHAL_USART_priv_startRx(USART_TypeDef *periph);

/// Disable the receiver and its DMA request line (CR1.RE, CR3.DMAR).
void PHAL_USART_priv_stopRx(USART_TypeDef *periph);

/// Discard anything sitting in RDR and clear IDLE plus every RX error flag.
void PHAL_USART_priv_flushRx(USART_TypeDef *periph);

/// @return true if the IDLE-line flag is set (an RX frame just completed).
bool PHAL_USART_priv_idleActive(USART_TypeDef *periph);

/// Clear the IDLE flag alone (write-1-to-clear).
void PHAL_USART_priv_clearIdle(USART_TypeDef *periph);

/// @return true if the slot's TX DMA channel signalled transfer complete.
bool PHAL_USART_priv_txDmaComplete(ssize_t idx);

/// Clear all interrupt flags for the slot's TX DMA channel.
void PHAL_USART_priv_clearTxDmaFlags(ssize_t idx);

void PHAL_USART_priv_enableIrqs(ssize_t idx);

#endif // __PHAL_G4_USART_PRIV_H__
