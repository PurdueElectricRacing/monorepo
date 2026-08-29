/**
 * @file spi.c
 * @author Ronak Jain (jain717@purdue.edu)
 * @author Shriya Balu (balu@purdue.edu)
 * @brief G4 SPI
 * @version 0.1
 */

#include "common/phal_G4/spi/spi.h"
#include "common/phal_G4/spi/spi_priv.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/dma/dma.h"
#include "common/utils/clamp.h"


static uint16_t trash_can; // For RX discard when in_data NULL
static uint16_t zero;      // For TX dummy when out_data NULL

[[gnu::weak]]
void PHAL_SPI_txCallback(SPI_InitConfig_t *spi) {
    (void)spi;
}

// DMA callback handler for TX completion
static void spi_tx_dma_callback(void *ctx) {
    PHAL_SPI_priv_handleTxComplete((SPI_InitConfig_t *)ctx);
}

bool PHAL_SPI_init(SPI_InitConfig_t *cfg) {
    // Enable RCC Clock for selected SPI on G4
    PHAL_SPI_priv_enableClock(cfg->periph);

    /// Peripheral configuration (See RM0440 42.5.7 Configuration of SPI section)
    uint32_t f_div = PHAL_SPI_priv_calcBaudRatePrescaler(cfg->data_rate, cfg->periph);

    // Write to the SPI_CR1 register (baud rate, CPOL/CPHA, simplex/half-duplex, frame format, CRC, slave select, master/slave configs)
    PHAL_SPI_priv_configCR1(cfg, f_div);

    // Write to SPI_CR2 register (transfer data length, slave select output enable, )
    PHAL_SPI_priv_configCR2(cfg);

    // DMA setup is required 
    if (!PHAL_DMA_init(cfg->rx_dma) || !PHAL_DMA_initWithCallback(cfg->tx_dma, spi_tx_dma_callback, cfg)) {
        return false;
    }

    // Deassert CS in master when using software NSS
    if (cfg->mode == SPI_MODE_MASTER && cfg->nss_sw) {
        PHAL_GPIO_write(cfg->nss_gpio_port, (uint8_t)cfg->nss_gpio_pin, 1);
    }

    PHAL_SPI_priv_resetTransferState(cfg);

    return true;
}


void PHAL_SPI_transfer(
    SPI_InitConfig_t *spi,
    const uint8_t *out_data,
    // DMA writes rx'd bytes into in_data async
    // cppcheck-suppress constParameterPointer
    uint8_t *in_data,
    uint16_t data_len
) {

    // Wait for any previous transfer to complete
    while (PHAL_SPI_busy(spi)) {
        __asm__("nop");
    }

    // Assert CS for master only
    if (spi->mode == SPI_MODE_MASTER && spi->nss_sw)
        PHAL_GPIO_write(spi->nss_gpio_port, (uint8_t)spi->nss_gpio_pin, 0);

    spi->_busy = true;

    // TX DMA enable
    PHAL_DMA_stop(spi->tx_dma);
    PHAL_SPI_priv_enableDMA_TX(spi);
    if (!out_data) {
        PHAL_DMA_setMemInc(spi->tx_dma, false);
        (void)PHAL_DMA_setMemAddress(spi->tx_dma, (uint32_t)&zero);
    } else {
        PHAL_DMA_setMemInc(spi->tx_dma, true);
        (void)PHAL_DMA_setMemAddress(spi->tx_dma, (uint32_t)out_data);
    }
    (void)PHAL_DMA_setLength(spi->tx_dma, data_len);

    // RX DMA
    PHAL_DMA_stop(spi->rx_dma);
    PHAL_SPI_priv_enableDMA_RX(spi);

    if (!in_data) {
        PHAL_DMA_setMemInc(spi->rx_dma, false);
        (void)PHAL_DMA_setMemAddress(spi->rx_dma, (uint32_t)&trash_can);
    } else {
        PHAL_DMA_setMemInc(spi->rx_dma, true);
        (void)PHAL_DMA_setMemAddress(spi->rx_dma, (uint32_t)in_data);
    }
    (void)PHAL_DMA_setLength(spi->rx_dma, data_len);
    PHAL_DMA_restart(spi->rx_dma);

    // Start SPI and kick TX DMA
    PHAL_SPI_priv_Enable(spi);
    PHAL_DMA_restart(spi->tx_dma);
}


void PHAL_SPI_transferBlocking(SPI_InitConfig_t *spi,
                       const uint8_t *out_data,
                       uint8_t *in_data,
                       uint16_t data_len) {
    // Start the transfer
    PHAL_SPI_transfer(spi, out_data, in_data, data_len);
    // Wait for this transfer to complete
    while (PHAL_SPI_busy(spi)) {
        __NOP();
    }
}

bool PHAL_SPI_busy(const SPI_InitConfig_t *cfg) {
    return cfg->_busy;
}
