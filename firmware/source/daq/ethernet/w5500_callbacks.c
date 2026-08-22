
/**
 * @file w5500_callbacks.c
 * @brief Implementation of the W5500 SPI callbacks for ethernet communication.
 *
 * @author Eileen Yoon (eyn@purdue.edu)
 */

#include "w5500/wizchip_conf.h"
#include "common/phal_F4/spi/spi.h"
#include "main.h"

extern SPI_InitConfig_t eth_spi_config;

static void cs_sel(void);
static void cs_desel(void);
static uint8_t spi_rb(uint32_t addr);
static void spi_wb(uint32_t addr, uint8_t b);
static void spi_rb_burst(uint32_t addr, uint8_t* pBuf, uint16_t len);
static void spi_wb_burst(uint32_t addr, uint8_t* pBuf, uint16_t len);
static void crit_enter(void);
static void crit_exit(void);
static void PHAL_SPI_transfer_noDMA_DAQW5500Only(SPI_InitConfig_t* spi, uint32_t addr, const uint8_t* tx_data, uint32_t txlen, uint32_t rxlen, uint8_t* rx_data);

/*
 * W5500 uses a custom framed multi-byte SPI format that requires
 * CS to be low for the entire duration of the multi-byte transaction.
 * Hence the SW CS. Since the W5500 driver pulls CS manually using this
 * callback before calling SPI_transfer, and the SPI peripheral needs to be
 * enabled before CS, we enable it here and use a special function in the PHAL
 * that doesn't pull CS/enable SPI
 */
static void cs_sel(void) {
    eth_spi_config.periph->CR1 |= SPI_CR1_SPE;
    PHAL_writeGPIO(ETH_CS_PORT, ETH_CS_PIN, 0);
}

static void cs_desel(void) {
    eth_spi_config.periph->CR1 &= ~SPI_CR1_SPE;
    PHAL_writeGPIO(ETH_CS_PORT, ETH_CS_PIN, 1);
}

// DAQ W5500 uses a custom framed multi-byte SPI format + software CS that makes this necessary
static void PHAL_SPI_transfer_noDMA_DAQW5500Only(SPI_InitConfig_t* spi, uint32_t addr, const uint8_t* tx_data, uint32_t txlen, uint32_t rxlen, uint8_t* rx_data) {
    // crit_enter() and crit_exit() in the W5500 callback
    // takes lock before entering this function, so this fn does not lock nor fail

    rx_data += txlen;

    // DO NOT Enable SPI here
    // The CS must come after enabling SPI, and since W5500 driver selects CS, we manually add SPE to the CS sel hook
    //spi->periph->CR1 |= SPI_CR1_SPE;

    // Add messages to TX FIFO
    // Write the first data item to be transmitted into the SPI_DR register (this clears the TXE flag).
    // The sequence begins when data are written into the SPI_DR register (Tx buffer).
    // 24-bit address selection (mandatory)
    spi->periph->DR = (addr & 0x00FF0000) >> 16;
    while (!(spi->periph->SR & SPI_SR_TXE))
        ;
    spi->periph->DR = (addr & 0x0000FF00) >> 8;
    while (!(spi->periph->SR & SPI_SR_TXE))
        ;
    spi->periph->DR = (addr & 0x000000FF) >> 0;
    while (!(spi->periph->SR & SPI_SR_TXE))
        ;
    for (uint32_t i = 0; i < txlen; i++) {
        spi->periph->DR = tx_data[i];
        while (!(spi->periph->SR & SPI_SR_TXE))
            ;
    }

    /*
     * During discontinuous communications, there is a 2 APB clock period delay
     * between the write operation to the SPI_DR register and BSY bit setting.
     * As a consequence it is mandatory to wait first until TXE is set and then
     * until BSY is cleared after writing the last data.
     */
    while (((spi->periph->SR) & SPI_SR_BSY))
        ;

    // Clear overrun
    (void)spi->periph->DR;
    (void)spi->periph->SR;

    // RX
    for (uint32_t i = 0; i < rxlen; i++) {
        // Wait till SPI is not in active transaction, send dummy
        while (((spi->periph->SR) & SPI_SR_BSY))
            ;
        spi->periph->DR = 0;

        // Wait for RX FIFO to recieve a message, read it in
        while (!(spi->periph->SR & SPI_SR_RXNE))
            ;
        rx_data[i] = (uint8_t)(spi->periph->DR);
    }

    // Do NOT wait until done, there is nothing to wait for (RX already received) and it will hang
    //Wait till transaction is finished, disable spi, and clear the queue
    //while ((spi->periph->SR & SPI_SR_BSY));
}

static uint8_t spi_rb(uint32_t addr) {
    uint8_t b;
    PHAL_SPI_transfer_noDMA_DAQW5500Only(&eth_spi_config, addr, NULL, 0, sizeof(b), &b);
    return b;
}

static void spi_wb(uint32_t addr, uint8_t b) {
    PHAL_SPI_transfer_noDMA_DAQW5500Only(&eth_spi_config, addr, &b, sizeof(b), 0, NULL);
}

static void spi_rb_burst(uint32_t addr, uint8_t* pBuf, uint16_t len) {
    // SPI RX Burst, must block! (uses local pointer)
    PHAL_SPI_transfer_noDMA_DAQW5500Only(&eth_spi_config, addr, NULL, 0, len, pBuf);
}

// WIZnet callback ABI does not mark pBuf as const, even though it is read only
// cppcheck-suppress constParameterCallback
static void spi_wb_burst(uint32_t addr, uint8_t* pBuf, uint16_t len) {
    // SPI TX Burst, must block! (uses local pointer)
    PHAL_SPI_transfer_noDMA_DAQW5500Only(&eth_spi_config, addr, pBuf, len, 0, NULL);
}

static void crit_enter(void) {
    if (xSemaphoreTake(spi1_lock, portMAX_DELAY) != pdTRUE) {
        HardFault_Handler(); // should not reach
    }
}

static void crit_exit(void) {
    xSemaphoreGive(spi1_lock);
}

void w5500_register_callbacks(void) {
    PHAL_writeGPIO(ETH_CS_PORT, ETH_CS_PIN, 1);
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
    reg_wizchip_spiburst_cbfunc(spi_rb_burst, spi_wb_burst);
    reg_wizchip_cris_cbfunc(crit_enter, crit_exit);
}
