/**
 * @file dma.c
 * @brief G4 DMA Peripheral public API implementation
 * @author Shriya Balu (balu@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/dma/dma.h"

#include "common/phal_G4/dma/dma_priv.h"

// Tracks which (periph, channel_idx) pairs are owned by a live handle.
// Doubles as (a) conflict prevention when claiming a channel and (b) the
// DMA PHAL's dispatch table for its own IRQ vectors below — this is what
// lets DMA own every DMAx_ChannelN_IRQHandler while other HALs just
// register a callback instead of defining competing vectors.
// Index 0 is unused (channel numbering is 1-8).
static PHAL_DMA_Handle_t *g_dma1_channel_owner[9];
static PHAL_DMA_Handle_t *g_dma2_channel_owner[9];

static PHAL_DMA_Handle_t **dma_channel_owner_slot(DMA_TypeDef *periph, uint8_t channel_idx) {
    PHAL_DMA_Handle_t **table = (periph == DMA1) ? g_dma1_channel_owner : g_dma2_channel_owner;
    return &table[channel_idx];
}

static bool dma_wiring_is_valid(const PHAL_DMA_Wiring_t *wiring) {
    if (wiring == nullptr || (wiring->periph != DMA1 && wiring->periph != DMA2)) {
        return false;
    }
    if (wiring->channel_idx < 1U || wiring->channel_idx > 8U) {
        return false;
    }
    return true;
}

bool PHAL_DMA_init(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || !dma_wiring_is_valid(handle->wiring)) {
        return false;
    }

    const PHAL_DMA_Wiring_t *wiring = handle->wiring;
    PHAL_DMA_Handle_t **owner = dma_channel_owner_slot(wiring->periph, wiring->channel_idx);
    if (*owner != nullptr) {
        // Already claimed by another handle
        return false;
    }

    PHAL_DMA_priv_enableClock(wiring->periph);
    handle->channel = PHAL_DMA_priv_getChannel(wiring->periph, wiring->channel_idx);

    PHAL_DMA_priv_disableChannel(handle->channel);
    PHAL_DMA_priv_clearFlags(wiring->periph, wiring->channel_idx);
    PHAL_DMA_priv_setPeriphAddress(handle->channel, (uint32_t)wiring->periph_reg);
    PHAL_DMA_priv_setMemAddress(handle->channel, handle->params.mem_addr);
    PHAL_DMA_priv_setLength(handle->channel, handle->params.tx_size);
    PHAL_DMA_priv_configChannel(handle->channel, wiring, &handle->params);

    // MEM2MEM transfers aren't triggered by a peripheral request line
    // No DMAMUX routing to configure
    if (handle->params.mode != DMA_MODE_MEM2MEM) {
        PHAL_DMA_priv_configMux(wiring);
    }

    *owner = handle;

    // If the user registered a callback, arm the NVIC line for this channel
    if (handle->callback.irq_fn != nullptr) {
        NVIC_EnableIRQ(PHAL_DMA_priv_getIRQn(wiring->periph, wiring->channel_idx));
    }

    return true;
}

bool PHAL_DMA_initWithCallback(PHAL_DMA_Handle_t *handle, PHAL_DMA_IRQCallbackFn_t irq_fn, void *ctx) {
    if (handle == nullptr) {
        return false;
    }
    handle->callback.irq_fn = irq_fn;
    handle->callback.ctx    = ctx;
    return PHAL_DMA_init(handle);
}

bool PHAL_DMA_deinit(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_disableChannel(handle->channel);

    if (handle->callback.irq_fn != nullptr) {
        NVIC_DisableIRQ(PHAL_DMA_priv_getIRQn(handle->wiring->periph, handle->wiring->channel_idx));
    }

    *dma_channel_owner_slot(handle->wiring->periph, handle->wiring->channel_idx) = nullptr;
    
    handle->channel = nullptr;
    handle->callback.irq_fn = nullptr;
    handle->callback.ctx = nullptr;
    
    return true;
}

bool PHAL_DMA_start(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_enableChannel(handle->channel);
    return true;
}

bool PHAL_DMA_stop(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_disableChannel(handle->channel);
    return true;
}

bool PHAL_DMA_restart(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_disableChannel(handle->channel);
    PHAL_DMA_priv_clearFlags(handle->wiring->periph, handle->wiring->channel_idx);
    PHAL_DMA_priv_setLength(handle->channel, handle->params.tx_size);
    PHAL_DMA_priv_enableChannel(handle->channel);
    return true;
}

bool PHAL_DMA_setMemAddress(PHAL_DMA_Handle_t *handle, uint32_t address) {
    if (handle == nullptr || handle->channel == nullptr || PHAL_DMA_priv_isChannelEnabled(handle->channel)) {
        return false;
    }

    handle->params.mem_addr = address;
    PHAL_DMA_priv_setMemAddress(handle->channel, address);
    return true;
}

bool PHAL_DMA_setLength(PHAL_DMA_Handle_t *handle, uint16_t length) {
    if (handle == nullptr || handle->channel == nullptr || PHAL_DMA_priv_isChannelEnabled(handle->channel)) {
        return false;
    }

    handle->params.tx_size = length;
    PHAL_DMA_priv_setLength(handle->channel, length);
    return true;
}

uint16_t PHAL_DMA_getRemaining(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return 0;
    }

    return PHAL_DMA_priv_getRemainingLength(handle->channel);
}

bool PHAL_DMA_isBusy(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    return PHAL_DMA_priv_isChannelEnabled(handle->channel);
}

bool PHAL_DMA_isComplete(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    return PHAL_DMA_priv_readCompleteFlag(handle->wiring->periph, handle->wiring->channel_idx);
}

bool PHAL_DMA_isError(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    return PHAL_DMA_priv_readErrorFlag(handle->wiring->periph, handle->wiring->channel_idx);
}

bool PHAL_DMA_clearFlags(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_clearFlags(handle->wiring->periph, handle->wiring->channel_idx);
    return true;
}

DMA_TypeDef *PHAL_DMA_getPeriph(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return nullptr;
    }

    return handle->wiring->periph;
}

uint8_t PHAL_DMA_getChannelIdx(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return 0;
    }

    return handle->wiring->channel_idx;
}

void PHAL_DMA_setMemInc(PHAL_DMA_Handle_t *handle, bool mem_inc) {
    if (handle == nullptr || handle->channel == nullptr || PHAL_DMA_priv_isChannelEnabled(handle->channel)) {
        return;
    }

    handle->params.mem_inc = mem_inc;
    PHAL_DMA_priv_configChannel(handle->channel, handle->wiring, &handle->params);
}


static void dma_dispatch(DMA_TypeDef *periph, uint8_t channel_idx) {
    PHAL_DMA_Handle_t *handle = *dma_channel_owner_slot(periph, channel_idx);
    if (handle != nullptr && handle->callback.irq_fn != nullptr) {
        handle->callback.irq_fn(handle->callback.ctx);
    }
}

void DMA1_Channel1_IRQHandler(void) { dma_dispatch(DMA1, 1); }
void DMA1_Channel2_IRQHandler(void) { dma_dispatch(DMA1, 2); }
void DMA1_Channel3_IRQHandler(void) { dma_dispatch(DMA1, 3); }
void DMA1_Channel4_IRQHandler(void) { dma_dispatch(DMA1, 4); }
void DMA1_Channel5_IRQHandler(void) { dma_dispatch(DMA1, 5); }
void DMA1_Channel6_IRQHandler(void) { dma_dispatch(DMA1, 6); }
void DMA1_Channel7_IRQHandler(void) { dma_dispatch(DMA1, 7); }

void DMA2_Channel1_IRQHandler(void) { dma_dispatch(DMA2, 1); }
void DMA2_Channel2_IRQHandler(void) { dma_dispatch(DMA2, 2); }
void DMA2_Channel3_IRQHandler(void) { dma_dispatch(DMA2, 3); }
void DMA2_Channel4_IRQHandler(void) { dma_dispatch(DMA2, 4); }
void DMA2_Channel5_IRQHandler(void) { dma_dispatch(DMA2, 5); }
