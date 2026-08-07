/**
 * @file dma.c
 * @brief G4 DMA Peripheral public API implementation
 * @author Shriya Balu (balu@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/dma/dma.h"

#include "common/phal_G4/dma/dma_priv.h"

// Tracks which (periph, channel_idx) pairs are currently claimed by a live handle,
// so two handles can never conflict fight over the same channel.
// Index 0 is unused (channel numbering is 1-8).

static bool g_dma1_channel_claimed[9];
static bool g_dma2_channel_claimed[9];



static bool *dma_channel_claim_slot(DMA_TypeDef *periph, uint8_t channel_idx) {
    bool *table = (periph == DMA1) ? g_dma1_channel_claimed : g_dma2_channel_claimed;
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
    bool *claimed = dma_channel_claim_slot(wiring->periph, wiring->channel_idx);
    if (*claimed) {
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

    *claimed = true;
    return true;
}

bool PHAL_DMA_deinit(PHAL_DMA_Handle_t *handle) {
    if (handle == nullptr || handle->channel == nullptr) {
        return false;
    }

    PHAL_DMA_priv_disableChannel(handle->channel);
    *dma_channel_claim_slot(handle->wiring->periph, handle->wiring->channel_idx) = false;
    handle->channel = nullptr;
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

    return PHAL_DMA_priv_getLength(handle->channel);
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
