/**
 * @file spi.c
 * @author Aditya Anand (anand89@purdue.edu) - Port of L4 HAL by Adam Busch (busch8@purdue.edu)
 * @brief PER Serial Peripheral Interface Device driver for STM32F4 
 * @version 0.1
 * @date 2023-12-4
 *
 * @copyright Copyright (c) 2021
 *
 */
#include "common/phal_F4/spi/spi.h"
#include "common/utils/clamp.h"

extern uint32_t APB2ClockRateHz;
extern uint32_t APB1ClockRateHz;
static volatile SPI_InitConfig_t* active_transfer = NULL;

static uint16_t trash_can; //!< Used as an address for DMA to dump data into
static uint16_t zero; //!< Used as a constant zero during transmissions

static inline uint32_t LOG2_DOWN(uint32_t x) {
    return 31U - (uint32_t)__builtin_clz(x);
}

bool PHAL_SPI_init(SPI_InitConfig_t* cfg) {
    zero = 0;
    // Enable RCC Clock - Add new cases for each SPI Peripheral, to enable their clocks
    if (cfg->periph == SPI1) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    } else if (cfg->periph == SPI2) {
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    } else {
        return false;
    }

    // Setup for Master, positive polarity
    cfg->periph->CR1 |= SPI_CR1_MSTR | SPI_CR1_SPE | SPI_CR1_SSM | SPI_CR1_SSI;
    cfg->periph->CR1 &= ~(SPI_CR1_CPOL);
    cfg->periph->CR2 &= ~(SPI_CR2_SSOE);

#ifdef STM32F407xx
    if (cfg->data_len != 8 && cfg->data_len != 16)
        return false;
    // Set data frame size for SPI transaction to 8/16 bits depending on user configuration
    cfg->periph->CR1 &= ~(SPI_CR1_DFF);
    cfg->periph->CR1 |= (uint32_t)(cfg->data_len != 8) << SPI_CR1_DFF_Pos;
#endif

    // Data Rate
    // Divisor is a power of 2, find the closest power of 2 limited to log2(256)
    uint32_t f_div = 0;
    if (cfg->periph == SPI1)
        f_div = LOG2_DOWN(APB2ClockRateHz / cfg->data_rate) - 1;
    // Both SPI2 and SPI3 are on APB1
    else
        f_div = LOG2_DOWN(APB1ClockRateHz / cfg->data_rate) - 1;

    f_div = CLAMP(f_div, 0, 0b111);
    cfg->periph->CR1 &= ~SPI_CR1_BR_Msk;
    cfg->periph->CR1 |= f_div << SPI_CR1_BR_Pos;

    // Setup DMA Streams if required
    if (cfg->rx_dma_cfg && !PHAL_initDMA(cfg->rx_dma_cfg))
        return false;

    if (cfg->tx_dma_cfg && !PHAL_initDMA(cfg->tx_dma_cfg))
        return false;

    // Ensure device CS is disabled
    PHAL_GPIO_write(cfg->nss_gpio_port, (uint8_t)cfg->nss_gpio_pin, 1);

    cfg->_busy              = false;
    cfg->_error             = false;
    cfg->_direct_mode_error = false;
    cfg->_fifo_overrun      = false;

    return true;
}

bool PHAL_SPI_transfer_noDMA(SPI_InitConfig_t* spi, const uint8_t* out_data, uint32_t txlen, uint32_t rxlen, uint8_t* in_data) {
    in_data += txlen;

    if (PHAL_SPI_busy(spi))
        return false;

    active_transfer = spi;
    spi->_busy      = true;
    // Enable SPI
    spi->periph->CR1 |= SPI_CR1_SPE;

    // Select peripheral
    if (spi->nss_sw)
        PHAL_GPIO_write(spi->nss_gpio_port, (uint8_t)spi->nss_gpio_pin, 0);
    // Add messages to TX FIFO
    for (uint32_t i = 0; i < txlen; i++) {
        while (!(spi->periph->SR & SPI_SR_TXE))
            ;
        // TODO: check byte size and set DR accordingly
        // if (i + 1 < txlen)
        // {
        //     uint16_t data = out_data[i + 1] << 8 | (uint16_t)out_data[i];
        //     spi->periph->DR = data;
        //     i++;
        // }
        // else
        // {
        spi->periph->DR = out_data[i];
        // }
    }
    // Wait till TX FIFO is empty and spi is not active
    while (!(spi->periph->SR & SPI_SR_TXE) || (spi->periph->SR & SPI_SR_BSY))
        ;
    // Clear overrun
    (void)spi->periph->DR;
    (void)spi->periph->SR;

    // RX
    for (uint32_t i = 0; i < rxlen; i++) {
        // Wait till SPI is not in active transaction, send dummy
        while ((spi->periph->SR & SPI_SR_BSY))
            ;
        spi->periph->DR = 0;
        // With for RX FIFO to recieve a message, read it in
        while (!(spi->periph->SR & SPI_SR_RXNE))
            ;
        in_data[i] = (uint8_t)(spi->periph->DR);
    }
    // Wait till transaction is finished, disable spi, and clear the queue
    while ((spi->periph->SR & SPI_SR_BSY))
        ;
    spi->periph->CR1 &= ~SPI_CR1_SPE;

    // Stop message by disabling CS
    if (spi->nss_sw)
        PHAL_GPIO_write(spi->nss_gpio_port, (uint8_t)spi->nss_gpio_pin, 1);

    spi->_busy = false;

    return true;
}

bool PHAL_SPI_transfer(SPI_InitConfig_t* spi, const uint8_t* out_data, const uint32_t data_len, const uint8_t* in_data) {
    /*
    Each DMA Stream is enabled if a data buffer is provided.
    RX Side interrupts disabled, since the same data lengths are sent to both TX and RX
    */
    // Cannot use DMA without knowing essential configuration info
    if (spi->tx_dma_cfg == 0) {
        return false;
    }

    if (PHAL_SPI_busy(spi))
        return false;

    active_transfer = spi;

    // Enable CS to begin SPI transaction
    if (spi->nss_sw)
        PHAL_GPIO_write(spi->nss_gpio_port, (uint8_t)spi->nss_gpio_pin, 0);

    spi->_busy = true;

    // Configure DMA send msg
    spi->periph->CR2 |= SPI_CR2_TXDMAEN;
    if (!out_data) {
        // No data to send, fill with dummy data
        spi->tx_dma_cfg->stream->CR &= ~DMA_SxCR_MINC;
        PHAL_DMA_setMemAddress(spi->tx_dma_cfg, (uint32_t)&zero);
    } else {
        PHAL_DMA_setMemAddress(spi->tx_dma_cfg, (uint32_t)out_data);
    }
    PHAL_DMA_setTxferLength(spi->tx_dma_cfg, data_len);

    //Configure DMA receive Msg
    if (spi->rx_dma_cfg) {
        spi->periph->CR2 |= SPI_CR2_RXDMAEN;
        if (!in_data) {
            // No data to receive, so configure DMA to disregard any received messages
            spi->rx_dma_cfg->stream->CR &= ~DMA_SxCR_MINC;
            PHAL_DMA_setMemAddress(spi->rx_dma_cfg, (uint32_t)&trash_can);
        } else {
            PHAL_DMA_setMemAddress(spi->rx_dma_cfg, (uint32_t)in_data);
        }
        PHAL_DMA_setTxferLength(spi->rx_dma_cfg, data_len);

        // We must clear interrupt flags before enabling DMA
        PHAL_DMA_reEnable(spi->rx_dma_cfg);
    }

    // Enable the DMA IRQ - copy + paste enabling selected SPI peripheral's TX DMA Stream ISR
    if (spi->periph == SPI1) {
        NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    }

    // Start transaction
    spi->periph->CR1 |= SPI_CR1_SPE;

    // STM32 HAL Libraries start TX Dma transaction last
    PHAL_DMA_reEnable(spi->tx_dma_cfg);

    return true;
}

bool PHAL_SPI_busy(SPI_InitConfig_t* cfg) {
    // Latch in case active_transfer cleared during interrupt
    volatile SPI_InitConfig_t* act = active_transfer;
    if (act && cfg->periph == act->periph)
        return act->_busy;

    return false;
}

void PHAL_SPI_ForceReset(SPI_InitConfig_t* spi) {
    if (spi->periph == SPI1) {
        RCC->APB2RSTR |= (RCC_APB2RSTR_SPI1RST);
        RCC->APB2RSTR &= ~(RCC_APB2RSTR_SPI1RST);
    } else if (spi->periph == SPI2) {
        RCC->APB1RSTR |= (RCC_APB1RSTR_SPI2RST);
        RCC->APB1RSTR &= ~(RCC_APB1RSTR_SPI2RST);
    }
}

/**
 * @brief Handle TCIF interrupt signaling end of TX transaction
 *
 */
static void PHAL_SPI_priv_handleTxComplete() {
    if (active_transfer == NULL) {
        return;
    }

    // Bitmask for each DMA interrupt flag
    uint32_t teif_flag;
    uint32_t tcif_flag;
    uint32_t htif_flag;
    uint32_t feif_flag;
    uint32_t dmeif_flag;

    // Clear register for DMA Stream
    volatile uint32_t* sr_reg;
    volatile uint32_t* csr_reg;

    // Populate Flag Bitmasks, along with Flag and Clear registers for active DMA Stream
    switch (active_transfer->tx_dma_cfg->stream_idx) {
        case 0:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF0;
            tcif_flag  = DMA_LISR_TCIF0;
            htif_flag  = DMA_LISR_HTIF0;
            feif_flag  = DMA_LISR_FEIF0;
            dmeif_flag = DMA_LISR_DMEIF0;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->LISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->LIFCR;
            break;
        case 1:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF1;
            tcif_flag  = DMA_LISR_TCIF1;
            htif_flag  = DMA_LISR_HTIF1;
            feif_flag  = DMA_LISR_FEIF1;
            dmeif_flag = DMA_LISR_DMEIF1;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->LISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->LIFCR;
            break;
        case 2:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF2;
            tcif_flag  = DMA_LISR_TCIF2;
            htif_flag  = DMA_LISR_HTIF2;
            feif_flag  = DMA_LISR_FEIF2;
            dmeif_flag = DMA_LISR_DMEIF2;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->LISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->LIFCR;
            break;
        case 3:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF3;
            tcif_flag  = DMA_LISR_TCIF3;
            htif_flag  = DMA_LISR_HTIF3;
            feif_flag  = DMA_LISR_FEIF3;
            dmeif_flag = DMA_LISR_DMEIF3;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->LISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->LIFCR;
            break;
        case 4:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF4;
            tcif_flag  = DMA_HISR_TCIF4;
            htif_flag  = DMA_HISR_HTIF4;
            feif_flag  = DMA_HISR_FEIF4;
            dmeif_flag = DMA_HISR_DMEIF4;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->HISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->HIFCR;
            break;
        case 5:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF5;
            tcif_flag  = DMA_HISR_TCIF5;
            htif_flag  = DMA_HISR_HTIF5;
            feif_flag  = DMA_HISR_FEIF5;
            dmeif_flag = DMA_HISR_DMEIF5;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->HISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->HIFCR;
            break;
        case 6:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF6;
            tcif_flag  = DMA_HISR_TCIF6;
            htif_flag  = DMA_HISR_HTIF6;
            feif_flag  = DMA_HISR_FEIF6;
            dmeif_flag = DMA_HISR_DMEIF6;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->HISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->HIFCR;
            break;
        case 7:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF7;
            tcif_flag  = DMA_HISR_TCIF7;
            htif_flag  = DMA_HISR_HTIF7;
            feif_flag  = DMA_HISR_FEIF7;
            dmeif_flag = DMA_HISR_DMEIF4;

            // Select appropriate flag and clear registers for Stream
            sr_reg  = &active_transfer->tx_dma_cfg->periph->HISR;
            csr_reg = &active_transfer->tx_dma_cfg->periph->HIFCR;
            break;
        default:
            // Invalid stream selected, do not attempt to service interrupt
            return;
    }

    bool clear_act_transfer = false;
    // Transfer Error interrupt
    if (*sr_reg & teif_flag) {
        *csr_reg |= teif_flag;
        active_transfer->_error = true;
    }
    // Transfer Complete interrupt flag
    if (*sr_reg & tcif_flag) {
        // RM0090 p.895, wait for TXE and BSY to be cleared to satisfy timing requirements
        while (!(active_transfer->periph->SR & (SPI_SR_TXE)) || (active_transfer->periph->SR & (SPI_SR_BSY)))
            ;
        // Raise CS to end transaction
        if (active_transfer->nss_sw)
            PHAL_GPIO_write(active_transfer->nss_gpio_port, (uint8_t)active_transfer->nss_gpio_pin, 1);

        // Disable DMA channels
        PHAL_DMA_stopTxfer(active_transfer->rx_dma_cfg);
        PHAL_DMA_stopTxfer(active_transfer->tx_dma_cfg);

        // Revert to mem_inc if trash_can used
        if (active_transfer->rx_dma_cfg->mem_inc)
            active_transfer->rx_dma_cfg->stream->CR |= DMA_SxCR_MINC;
        if (active_transfer->tx_dma_cfg->mem_inc)
            active_transfer->tx_dma_cfg->stream->CR |= DMA_SxCR_MINC;

        // Disable SPI peripheral and DMA requests
        active_transfer->periph->CR1 &= ~SPI_CR1_SPE;
        active_transfer->periph->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);

        // Clear possible errors, remove current transaction from active transfer
        active_transfer->_busy              = false;
        active_transfer->_error             = false;
        active_transfer->_direct_mode_error = false;
        active_transfer->_fifo_overrun      = false;
        clear_act_transfer                  = true;

        //Clear interrupt flag
        *csr_reg |= tcif_flag;
    }
    // Half transfer complete flag
    if ((*sr_reg) & htif_flag) {
        *csr_reg |= htif_flag;
    }
    // FIFO Overrun Error flag
    if (*sr_reg & feif_flag) {
        active_transfer->_fifo_overrun = true;
        *csr_reg |= feif_flag;
    }
    // Direct Mode Error flag
    if (*sr_reg & dmeif_flag) {
        active_transfer->_direct_mode_error = true;
        *csr_reg |= dmeif_flag;
    }

    // Clear active transfer
    if (clear_act_transfer)
        active_transfer = NULL;
}

//DMA TX ISR - copy + paste for selected SPI peripheral's DMA ISR
void DMA2_Stream3_IRQHandler() {
    PHAL_SPI_priv_handleTxComplete();
}

uint8_t PHAL_SPI_readByte(SPI_InitConfig_t* spi, uint8_t address, bool skipDummy) {
    static uint8_t tx_cmd[4] = {(1U << 7), 0, 0};
    static uint8_t rx_dat[4] = {1, 1, 1, 1};
    tx_cmd[0] |= (address & 0x7F);

    // Send address, read byte depending on whether DMA is selected
    while (PHAL_SPI_busy(spi))
        ;
    if (spi->rx_dma_cfg != NULL)
        PHAL_SPI_transfer(spi, tx_cmd, skipDummy ? 2 : 3, rx_dat);
    else
        PHAL_SPI_transfer_noDMA(spi, tx_cmd, 1, skipDummy ? 1 : 2, rx_dat);
    while (PHAL_SPI_busy(spi))
        ;

    // Skip first byte of rx in case of unnecesssary information
    return skipDummy ? rx_dat[1] : rx_dat[2];
}

uint8_t PHAL_SPI_writeByte(SPI_InitConfig_t* spi, uint8_t address, uint8_t writeDat) {
    uint8_t tx_cmd[3] = {0};
    uint8_t rx_dat[3] = {0};
    tx_cmd[0] |= (address & 0x7F);
    tx_cmd[1] |= (writeDat);

    // Send address, write byte depending on whether DMA is selected
    while (PHAL_SPI_busy(spi))
        ;
    if (spi->tx_dma_cfg != NULL)
        PHAL_SPI_transfer(spi, tx_cmd, 2, rx_dat);
    else
        PHAL_SPI_transfer_noDMA(spi, tx_cmd, 2, 0, rx_dat);
    while (PHAL_SPI_busy(spi))
        ;

    return rx_dat[1];
}