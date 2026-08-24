/**
 * @file spi_priv.c 
 * @brief G4 SPI Peripheral private/register level implementation 
 * @author Shriya Balu (balu@purdue.edu)
 * @author Ronak Jain (jain717@purdue.edu)
 * 
 */

#include "common/phal_G4/spi/spi.h"
#include "common/phal_G4/spi/spi_priv.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/clamp.h"


static inline uint32_t LOG2_DOWN(uint32_t x) {
    return 31U - (uint32_t)__builtin_clz(x);
}

void PHAL_SPI_priv_enableClock(const SPI_TypeDef *periph) {
    if (periph == SPI1) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    } else if (periph == SPI2) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN;
    } else if (periph == SPI3) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_SPI3EN;
    } else {
        __builtin_trap();
    }
}

void PHAL_SPI_priv_configCR1(SPI_InitConfig_t *cfg, uint32_t f_div) {
    if (cfg->mode == SPI_MODE_MASTER) {
        // Master mode, software NSS
        cfg->periph->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
        cfg->periph->CR1 &= ~SPI_CR1_BR_Msk;
        cfg->periph->CR1 |= f_div << SPI_CR1_BR_Pos;
    } else {
        // Slave mode: clear MSTR.
        cfg->periph->CR1 &= ~SPI_CR1_MSTR;
        if (cfg->nss_sw) {
            // Software NSS: internally select the slave (SSM=1, SSI=0)
            cfg->periph->CR1 |= SPI_CR1_SSM;
            cfg->periph->CR1 &= ~SPI_CR1_SSI;
        } else {
            // Hardware NSS: NSS managed by external pin (SSM=0). SSI ignored.
            cfg->periph->CR1 &= ~SPI_CR1_SSM;
        }
        // BR ignored in slave
    }
    cfg->periph->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);
    if (cfg->cpol) {
        cfg->periph->CR1 |= SPI_CR1_CPOL;
    }
    if (cfg->cpha) {
        cfg->periph->CR1 |= SPI_CR1_CPHA;
    }
}

uint32_t PHAL_SPI_priv_calcBaudRatePrescaler(uint32_t data_rate, const SPI_TypeDef *periph) {
    /// See RM0440 42.9.1 SPI control register 1 (SPIx_CR1) 
    // BR prescaler in CR1 takes values 0b000 to 0b111, which correspond to divisors of 2, 4, 8, 16, 32, 64, 128, and 256.
    // This function calculates the BR value for a given data rate and SPI peripheral.
    uint32_t f_div;
    if ((uint32_t)periph == SPI1_BASE) {
        f_div = LOG2_DOWN(PHAL_RCC_getAPB2ClockHz() / data_rate) - 1;
    } else {
        f_div = LOG2_DOWN(PHAL_RCC_getAPB1ClockHz() / data_rate) - 1;
    }
    f_div = CLAMP(f_div, 0, 0b111);
    return f_div;
} 

void PHAL_SPI_priv_configCR2(SPI_InitConfig_t *cfg) {
    // Frame size via CR2 DS[3:0] on G4
    cfg->periph->CR2 &= ~(SPI_CR2_DS_Msk);
    uint8_t ds = (CLAMP(cfg->data_len, 4, 16) - 1) & 0xF; // DS = bits-1
    cfg->periph->CR2 |= (ds << SPI_CR2_DS_Pos);
    // RX FIFO threshold: set FRXTH for 8-bit threshold
    cfg->periph->CR2 |= SPI_CR2_FRXTH;
}

void PHAL_SPI_priv_enableDMA_TX(SPI_InitConfig_t *cfg) {
    cfg->periph->CR2 |= SPI_CR2_TXDMAEN;
}

void PHAL_SPI_priv_enableDMA_RX(SPI_InitConfig_t *cfg) {
    cfg->periph->CR2 |= SPI_CR2_RXDMAEN;
}

void PHAL_SPI_priv_handleTxComplete(SPI_InitConfig_t *transfer) {
    if (transfer == NULL || transfer->tx_dma == NULL) {
        return;
    }

    if (PHAL_DMA_isError(transfer->tx_dma)) {
        transfer->_error = true;
    }

    if (PHAL_DMA_isComplete(transfer->tx_dma)) {
        // Wait for TXE and not busy
        while (!(transfer->periph->SR & SPI_SR_TXE) || (transfer->periph->SR & SPI_SR_BSY)) {
            __NOP();
        }
        // If RX DMA is used, wait until its TC flag also asserts before teardown
        if (transfer->rx_dma) {
            // Busy-wait for RX complete
            while (!PHAL_DMA_isComplete(transfer->rx_dma)) {
                __NOP();
            }
            // Clear RX flags and stop RX
            PHAL_DMA_clearFlags(transfer->rx_dma);
            PHAL_DMA_stop(transfer->rx_dma);
        }

        // Deassert CS after both TX and RX complete
        if (transfer->nss_sw) {
            PHAL_GPIO_write(transfer->nss_gpio_port, transfer->nss_gpio_pin, 1);
        }

        if (transfer->tx_dma) {
            PHAL_DMA_stop(transfer->tx_dma);
        }

        transfer->periph->CR1 &= ~SPI_CR1_SPE;
        transfer->periph->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);

        PHAL_SPI_priv_resetTransferState(transfer);

        PHAL_SPI_txCallback(transfer);
    }

    PHAL_DMA_clearFlags(transfer->tx_dma);
}

void PHAL_SPI_priv_resetTransferState(SPI_InitConfig_t *cfg) {
    cfg->_busy              = false;
    cfg->_error             = false;
    cfg->_direct_mode_error = false;
    cfg->_fifo_overrun      = false;
}


void PHAL_SPI_priv_Enable(SPI_InitConfig_t *spi) {
    spi->periph->CR1 |= SPI_CR1_SPE;
}

void PHAL_SPI_priv_Disable(SPI_InitConfig_t *spi) {
    spi->periph->CR1 &= ~SPI_CR1_SPE;
}
