/**
 * @file dma_priv.h
 * @brief G4 DMA Peripheral private/register level implementation
 * @author Shriya Balu (balu@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PHAL_G4_DMA_PRIV_H
#define PHAL_G4_DMA_PRIV_H

#include "common/phal_G4/dma/dma.h"


// Every DMA1/DMA2 channel register block is this many bytes apart
// (RM0440 11: CCR, CNDTR, CPAR, CMAR, plus one reserved word = 20 bytes)
static constexpr uint32_t PHAL_DMA_PRIV_DMA_CHANNEL_MEM_STRIDE = 0x14U;

/// Enable DMAMUX1's clock plus the given DMA peripheral's clock
void PHAL_DMA_priv_enableClock(DMA_TypeDef *periph);

/// Look up the channel instance for (periph, channel_idx). channel_idx must be 1-8
DMA_Channel_TypeDef *PHAL_DMA_priv_getChannel(DMA_TypeDef *periph, uint8_t channel_idx);

/// Disable a channel and block until hardware confirms it is off
void PHAL_DMA_priv_disableChannel(DMA_Channel_TypeDef *channel);

/// Enable a channel, starting its transfer
void PHAL_DMA_priv_enableChannel(DMA_Channel_TypeDef *channel);

/// True if the channel is currently enabled
bool PHAL_DMA_priv_isChannelEnabled(DMA_Channel_TypeDef *channel);

/// Clear every latched status flag (global/complete/half/error) for one channel
void PHAL_DMA_priv_clearFlags(DMA_TypeDef *periph, uint8_t channel_idx);

/// Configure CCR (data sizes, priority, increment modes, mode, ISR enables) from wiring+params
void PHAL_DMA_priv_configChannel(DMA_Channel_TypeDef *channel,
	                             const PHAL_DMA_Wiring_t *wiring,
								 const PHAL_DMA_Params_t *params);

/// Route wiring->mux_request to (periph, channel_idx)'s DMAMUX channel
void PHAL_DMA_priv_configMux(const PHAL_DMA_Wiring_t *wiring);

/// Write the peripheral-side address (CPAR)
void PHAL_DMA_priv_setPeriphAddress(DMA_Channel_TypeDef *channel, uint32_t address);

/// Write the memory-side address (CMAR)
void PHAL_DMA_priv_setMemAddress(DMA_Channel_TypeDef *channel, uint32_t address);

/// Write the transfer length (CNDTR)
void PHAL_DMA_priv_setLength(DMA_Channel_TypeDef *channel, uint16_t length);

/// Read how many elements are still outstanding (CNDTR)
uint16_t PHAL_DMA_priv_getLength(DMA_Channel_TypeDef *channel);

/// True if the transfer-complete flag is currently set for (periph, channel_idx)
bool PHAL_DMA_priv_readCompleteFlag(DMA_TypeDef *periph, uint8_t channel_idx);

/// True if the transfer-error flag is currently set for (periph, channel_idx)
bool PHAL_DMA_priv_readErrorFlag(DMA_TypeDef *periph, uint8_t channel_idx);

#endif // PHAL_G4_DMA_PRIV_H