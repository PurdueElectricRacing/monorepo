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


/// Enable the peripheral clock for an SPI instance.
void PHAL_SPI_priv_enableClock(const SPI_TypeDef *periph);

/// Configure the SPI CR1 register.
void PHAL_SPI_priv_configCR1(SPI_InitConfig_t *cfg, uint32_t f_div);

/// Calculate the SPI baud-rate prescaler.
uint32_t PHAL_SPI_priv_calcBaudRatePrescaler(uint32_t data_rate, const SPI_TypeDef *periph);

/// Configure the SPI CR2 register.
void PHAL_SPI_priv_configCR2(SPI_InitConfig_t *cfg);

/// Enable SPI transmit DMA requests.
void PHAL_SPI_priv_enableDMA_TX(SPI_InitConfig_t *cfg);

/// Enable SPI receive DMA requests.
void PHAL_SPI_priv_enableDMA_RX(SPI_InitConfig_t *cfg);

/// Handle DMA transmit-complete interrupts.
void PHAL_SPI_priv_handleTxComplete(SPI_InitConfig_t *transfer);

/// Reset the internal SPI transfer state.
void PHAL_SPI_priv_resetTransferState(SPI_InitConfig_t *cfg);

/// Enable the SPI peripheral.
void PHAL_SPI_priv_Enable(SPI_InitConfig_t *spi);

/// Disable the SPI peripheral.
void PHAL_SPI_priv_Disable(SPI_InitConfig_t *spi);

#endif // _PHAL_G4_SPI_PRIV_H
