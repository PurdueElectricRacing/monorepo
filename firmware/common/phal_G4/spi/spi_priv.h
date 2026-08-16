/**
 * @file spi_priv.h
 * @author Shriya Balu (balu@purdue.edu)
 * @author Ronak Jain (jain717@purdue.edu)
 * @brief Internal G4 SPI helpers and DMA configuration macros.
 */

#ifndef _PHAL_G4_SPI_PRIV_H
#define _PHAL_G4_SPI_PRIV_H

#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/spi/spi.h"
#include "common/phal_G4/gpio/gpio.h"


/** @brief Compute the DMA flag mask for a channel. */
#define DMA_FLAG_MASK(flag_base, channel) (((uint32_t)(flag_base)) << (4 * ((uint32_t)(channel) - 1)))

/** @brief DMA transfer-complete flag mask. */
#define DMA_TCIF_MASK(channel) DMA_FLAG_MASK(DMA_ISR_TCIF1, (channel))

/** @brief DMA transfer-error flag mask. */
#define DMA_TEIF_MASK(channel) DMA_FLAG_MASK(DMA_ISR_TEIF1, (channel))

/** @brief DMA half-transfer flag mask. */
#define DMA_HTIF_MASK(channel) DMA_FLAG_MASK(DMA_ISR_HTIF1, (channel))

/** @brief DMA global interrupt flag mask. */
#define DMA_GIF_MASK(channel)  DMA_FLAG_MASK(DMA_ISR_GIF1,  (channel))

/**
 * @brief Enable the peripheral clock for an SPI instance.
 */
void PHAL_SPI_priv_enableClock(SPI_TypeDef *periph);

/**
 * @brief Configure the SPI CR1 register.
 */
void PHAL_SPI_priv_configCR1(SPI_InitConfig_t *cfg, uint32_t f_div);

/**
 * @brief Calculate the SPI baud-rate prescaler.
 */
uint32_t PHAL_SPI_priv_calcBaudRatePrescaler(uint32_t data_rate, SPI_TypeDef *periph);

/**
 * @brief Configure the SPI CR2 register.
 */
void PHAL_SPI_priv_configCR2(SPI_InitConfig_t *cfg);

/**
 * @brief Enable SPI transmit DMA requests.
 */
void PHAL_SPI_priv_enableDMA_TX(SPI_InitConfig_t *cfg);

/**
 * @brief Enable SPI receive DMA requests.
 */
void PHAL_SPI_priv_enableDMA_RX(SPI_InitConfig_t *cfg);

/**
 * @brief Handle DMA transmit-complete interrupts.
 */
void PHAL_SPI_priv_handleTxComplete(SPI_InitConfig_t *transfer);

/**
 * @brief Reset the internal SPI transfer state.
 */
void PHAL_SPI_priv_resetTransferState(SPI_InitConfig_t *cfg);

/**
 * @brief Register an active SPI transmit transfer.
 */
void PHAL_SPI_priv_registerActiveTx(SPI_InitConfig_t *spi);

/**
 * @brief Enable the SPI peripheral.
 */
void PHAL_SPI_priv_Enable(SPI_InitConfig_t *spi);

/**
 * @brief Disable the SPI peripheral.
 */
void PHAL_SPI_priv_Disable(SPI_InitConfig_t *spi);
#endif /* _PHAL_G4_SPI_PRIV_H */
