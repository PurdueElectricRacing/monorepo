/**
 * @file dma.c
 * @author Aditya Anand, Chris McGalliard - Port of L4 HAL by Dawson Moore (moore800@purdue.edu)
 * @brief Basic DMA Peripheral HAL library for setting up DMA transfers
 * @version 0.1
 * @date 2023-08-19
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "common/phal_F4/dma/dma.h"

bool PHAL_initDMA(dma_init_t* init) {
    // Check we aren't going to break the peripheral
    if (init->mem_to_mem && init->circular) {
        return false;
    } else if (init->dir > 1) {
        return false;
    } else if (init->priority > 3) {
        return false;
    } else if (init->mem_size > 2 || init->periph_size > 2) {
        return false;
    }

    // Enable clock in RCC
    if (init->periph == DMA1) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    } else if (init->periph == DMA2) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    } else {
        return false;
    }

    // Ensure the stream is disabled, must be in order to configure the DMA control registers
    init->stream->CR &= ~(DMA_SxCR_EN);
    while (init->stream->CR &= DMA_SxCR_EN)
        ;

    // Clear any stream dedicated status flags that may have been set previously
    switch (init->stream_idx) {
        case 0:
            init->periph->LIFCR |= 0x2D;
            break;
        case 1:
            init->periph->LIFCR |= 0x2D << 6;
            break;
        case 2:
            init->periph->LIFCR |= 0x2D << 16;
            break;
        case 3:
            init->periph->LIFCR |= 0x2D << 22;
            break;
        case 4:
            init->periph->HIFCR |= 0x2D;
            break;
        case 5:
            init->periph->HIFCR |= 0x2D << 6;
            break;
        case 6:
            init->periph->HIFCR |= 0x2D << 16;
            break;
        case 7:
            init->periph->HIFCR |= 0x2D << 22;
            break;
    }

    // Set peripheral port register address
    init->stream->PAR = init->periph_addr;

    // Set memory address
    init->stream->M0AR = init->mem_addr;

    init->stream->CR = 0; //Reset preconfigured CR values

    // Set channel, priority, memory data size
    init->stream->CR |= (init->dma_chan_request << DMA_SxCR_CHSEL_Pos)
        | (init->priority << DMA_SxCR_PL_Pos) | (init->mem_size << DMA_SxCR_MSIZE_Pos);

    // Set peripheral data size, memory increment, peripheral increment
    init->stream->CR |= (init->periph_size << DMA_SxCR_PSIZE_Pos)
        | (init->mem_inc << DMA_SxCR_MINC_Pos) | (init->periph_inc << DMA_SxCR_PINC_Pos);

    // Set circular mode, direction, transfer error interrupt enable, and transfer complete interrupt enable
    init->stream->CR |= (init->circular << DMA_SxCR_CIRC_Pos)
        | (init->dir << DMA_SxCR_DIR_Pos) | (init->tx_isr_en << DMA_SxCR_TCIE_Pos) | (init->tx_isr_en << DMA_SxCR_TEIE_Pos);

    // Set stream memory configuration
    PHAL_DMA_setTxferLength(init, init->tx_size);

    return true;
}

void PHAL_DMA_startTxfer(dma_init_t* init) {
    // Stream enable starts txfer
    init->stream->CR |= DMA_SxCR_EN;
}

void PHAL_DMA_stopTxfer(dma_init_t* init) {
    // Stream disable stops txfer
    init->stream->CR &= ~DMA_SxCR_EN;
}

void PHAL_DMA_reEnable(dma_init_t* init) {
    // Clear any stream dedicated status flags that may have been set previously
    switch (init->stream_idx) {
        case 0:
            init->periph->LIFCR |= 0x2D;
            break;
        case 1:
            init->periph->LIFCR |= 0x2D << 6;
            break;
        case 2:
            init->periph->LIFCR |= 0x2D << 16;
            break;
        case 3:
            init->periph->LIFCR |= 0x2D << 22;
            break;
        case 4:
            init->periph->HIFCR |= 0x2D;
            break;
        case 5:
            init->periph->HIFCR |= 0x2D << 6;
            break;
        case 6:
            init->periph->HIFCR |= 0x2D << 16;
            break;
        case 7:
            init->periph->HIFCR |= 0x2D << 22;
            break;
    }

    init->stream->CR |= DMA_SxCR_EN;
}

void PHAL_DMA_setMemAddress(dma_init_t* init, const uint32_t address) {
    init->stream->M0AR = address;
}

void PHAL_DMA_setTxferLength(dma_init_t* init, const uint32_t length) {
    init->stream->NDTR &= ~(DMA_SxNDT); // Clear current tx length
    init->stream->NDTR |= length; // Set number of data to transfer
}