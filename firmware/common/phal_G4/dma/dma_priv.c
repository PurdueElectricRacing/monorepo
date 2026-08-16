/**
 * @file dma_priv.c
 * @brief G4 DMA Peripheral private/register level implementation
 * @author Shriya Balu (balu@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/dma/dma_priv.h"

void PHAL_DMA_priv_enableClock(DMA_TypeDef *periph) {
    // AHB1ENR = AHB1 peripheral clock Enable Register
    // DMAMUX1EN = DMAMUX1 clock Enable bit
    // - DMAMUX1 routes peripheral requests to DMA channels
    // - required regardless of which DMA controller (DMA1/DMA2) is used
    RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;

    // DMA1EN / DMA2EN = DMA1/DMA2 clock Enable bit
    if (periph == DMA1) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    } else if (periph == DMA2) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    }
}

DMA_Channel_TypeDef *PHAL_DMA_priv_getChannel(DMA_TypeDef *periph, uint8_t channel_idx) {
    // Channels sit back-to-back in memory, PHAL_DMA_PRIV_DMA_CHANNEL_MEM_STRIDE apart
    // Channel numbering is 1-based in the manual, so subtract 1 to get index
    DMA_Channel_TypeDef *base = (periph == DMA1) ? DMA1_Channel1 : DMA2_Channel1;
    return (DMA_Channel_TypeDef *)((uint8_t *)base + PHAL_DMA_PRIV_DMA_CHANNEL_MEM_STRIDE * (channel_idx - 1U));
}

IRQn_Type PHAL_DMA_priv_getIRQn(DMA_TypeDef *periph, uint8_t channel_idx) {
    IRQn_Type base = (periph == DMA1) ? DMA1_Channel1_IRQn : DMA2_Channel1_IRQn;
    return (IRQn_Type)(base + channel_idx - 1);
}

void PHAL_DMA_priv_disableChannel(DMA_Channel_TypeDef *channel) {
    // CCR = Channel Configuration Register
    // EN = channel Enable bit
    channel->CCR &= ~DMA_CCR_EN;

    // Busy-wait until hardware confirms EN has actually cleared before returnin
    // Note: CNDTR/CMAR/CPAR are only writable while EN = 0
    while (channel->CCR & DMA_CCR_EN) {
        __asm__("nop");
    }
}

void PHAL_DMA_priv_enableChannel(DMA_Channel_TypeDef *channel) {
    // Setting EN (channel Enable bit) starts the transfer immediately if
    // its DMAMUX request line is already asserted or arms it to start
    // on the next request
    channel->CCR |= DMA_CCR_EN;
}

bool PHAL_DMA_priv_isChannelEnabled(DMA_Channel_TypeDef *channel) {
    return (channel->CCR & DMA_CCR_EN) != 0;
}

void PHAL_DMA_priv_clearFlags(DMA_TypeDef *periph, uint8_t channel_idx) {
    // ISR = Interrupt Status Register
    // IFCR = Interrupt Flag Clear Register
    // - write-1-to-clear, same bit layout as ISR
    // GIF/TCIF/HTIF/TEIF = Global/Transfer-Complete/Half-Transfer/Transfer-Error Interrupt Flag
    // - each channel's flags occupy a 4-bit group:
    //   - channel 1 = bits[3:0], channel 2 = bits[7:4], etc
    uint32_t shift = 4U * (channel_idx - 1U);
    periph->IFCR = (DMA_ISR_GIF1 | DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1) << shift;
}

bool PHAL_DMA_priv_readCompleteFlag(DMA_TypeDef *periph, uint8_t channel_idx) {
    // TCIF = Transfer-Complete Interrupt Flag
    // - set by hardware once the channel's element count reaches zero
    uint32_t shift = 4U * (channel_idx - 1U);
    return (periph->ISR & (DMA_ISR_TCIF1 << shift)) != 0;
}

bool PHAL_DMA_priv_readErrorFlag(DMA_TypeDef *periph, uint8_t channel_idx) {
    // TEIF = Transfer-Error Interrupt Flag
    // - set by hardware on a bus error during the transfer
    uint32_t shift = 4U * (channel_idx - 1U);
    return (periph->ISR & (DMA_ISR_TEIF1 << shift)) != 0;
}

static uint32_t modeBits(PHAL_DMA_Mode_t mode) {
    switch (mode) {
        case DMA_MODE_NORMAL:   return 0;
        case DMA_MODE_CIRCULAR: return DMA_CCR_CIRC;
        case DMA_MODE_MEM2MEM:  return DMA_CCR_MEM2MEM;
        default:
            __builtin_unreachable();
    }
}

void PHAL_DMA_priv_configChannel(DMA_Channel_TypeDef *channel,
                                 const PHAL_DMA_Wiring_t *wiring,
                                 const PHAL_DMA_Params_t *params) {
    bool mem2mem = params->mode == DMA_MODE_MEM2MEM;

    // MINC/PINC = Memory/Peripheral increment mode bit
    uint32_t minc = params->mem_inc ? DMA_CCR_MINC : 0;
    uint32_t pinc = mem2mem ? DMA_CCR_PINC : 0;

    channel->CCR =
          ((wiring->data_size << DMA_CCR_MSIZE_Pos) & DMA_CCR_MSIZE_Msk) // MSIZE = Memory data SIZE field
        | ((wiring->data_size << DMA_CCR_PSIZE_Pos) & DMA_CCR_PSIZE_Msk) // PSIZE = Peripheral data SIZE field
        | ((params->priority << DMA_CCR_PL_Pos) & DMA_CCR_PL_Msk)        // PL = channel Priority Level field
        | minc
        | pinc
        | ((wiring->dir << DMA_CCR_DIR_Pos) & DMA_CCR_DIR_Msk)           // DIR = data transfer DIRection bit
        | (params->tx_isr_en ? (DMA_CCR_TEIE | DMA_CCR_TCIE) : 0)        // TEIE/TCIE = Transfer-Error/-Complete Interrupt Enable
        | modeBits(params->mode);                                        // CIRC/MEM2MEM bits (NORMAL leaves both clear)
}

void PHAL_DMA_priv_configMux(const PHAL_DMA_Wiring_t *wiring) {
    // DMAMUX1 has one shared bank of request-routing channels
    // indices 0-7 feed DMA1 channels 1-8, indices 8-15 feed DMA2 channels 1-8
    // DMA channel numbering is 1-based, DMAMUX indexing is 0-based, so subtract 1 from channel_idx
    uint8_t mux_idx =
        (wiring->periph == DMA2) ? (wiring->channel_idx - 1U + 8U) : (wiring->channel_idx - 1U);
    DMAMUX_Channel_TypeDef *mux = DMAMUX1_Channel0 + mux_idx;

    // DMAREQ_ID = DMA REQuest ID field
    // - selects which peripheral's request line feeds this DMAMUX channel
    mux->CCR = (mux->CCR & ~DMAMUX_CxCR_DMAREQ_ID_Msk) | wiring->mux_request;
}

void PHAL_DMA_priv_setPeriphAddress(DMA_Channel_TypeDef *channel, uint32_t address) {
    // CPAR = Channel Peripheral Address Register
    channel->CPAR = address;
}

void PHAL_DMA_priv_setMemAddress(DMA_Channel_TypeDef *channel, uint32_t address) {
    // CMAR = Channel Memory Address Register
    channel->CMAR = address;
}

void PHAL_DMA_priv_setLength(DMA_Channel_TypeDef *channel, uint16_t length) {
    // CNDTR = Channel Number of Data to Transfer Register
    channel->CNDTR = length;
}

uint16_t PHAL_DMA_priv_getRemainingLength(DMA_Channel_TypeDef *channel) {
    // Hardware decrements CNDTR after every element, so it reads back as the
    // count still outstanding rather than the count originally programmed
    return (uint16_t)channel->CNDTR;
}