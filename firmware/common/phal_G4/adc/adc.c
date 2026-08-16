/**
 * @file adc.c
 * @brief G4 ADC public API implementation. All register-level detail lives
 *        in adc_priv.c; this file only coordinates the ADC with its DMA channel.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "common/phal_G4/adc/adc.h"

#include "common/phal_G4/adc/adc_priv.h"

// One active handle per ADC instance, so the DMA interrupt can find the
// handle that owns the channel that fired.
static PHAL_ADC_Handle_t *g_active_adc[4];


// Forward declaration of the DMA callback handler for ADC completion
static void adc_dma_callback(void *ctx);


/// Map a supported ADC instance to its zero-based active-handle slot.
static uint8_t adc_instance_index(ADC_TypeDef *instance) {
    if (instance == ADC1) {
        return 0;
    }
    if (instance == ADC2) {
        return 1;
    }
    if (instance == ADC3) {
        return 2;
    }
    return 3; // ADC4
}

/// Return the fixed DMA wiring assigned to an ADC instance.
static const PHAL_DMA_Wiring_t *adc_dma_wiring(ADC_TypeDef *instance) {
    switch (adc_instance_index(instance)) {
        case 0: return &ADC1_DMA_WIRING;
        case 1: return &ADC2_DMA_WIRING;
        case 2: return &ADC3_DMA_WIRING;
        default: return &ADC4_DMA_WIRING;
    }
}

/// Validate a public ADC configuration before touching hardware.
static bool adc_config_is_valid(const PHAL_ADC_Config_t *config) {
    if (config == nullptr || config->channels == nullptr) {
        return false;
    }
    if (!ADC_PRIV_instance_is_supported(config->instance)) {
        return false;
    }
    if (config->channel_count == 0U || config->channel_count > PHAL_ADC_MAX_CHANNEL_COUNT) {
        return false;
    }
    for (size_t i = 0U; i < config->channel_count; i++) {
        uint8_t channel = config->channels[i].channel;
        if (channel < 1U || channel > PHAL_ADC_MAX_CHANNEL_NUMBER) {
            return false;
        }
    }
    return true;
}

/// Return the NVIC interrupt assigned to an ADC instance's DMA channel.
static IRQn_Type adc_dma_irqn(ADC_TypeDef *instance) {
    const PHAL_DMA_Wiring_t *wiring = adc_dma_wiring(instance);
    // DMA channel IRQ numbers are consecutive within each controller.
    IRQn_Type base = (wiring->periph == DMA1) ? DMA1_Channel1_IRQn : DMA2_Channel1_IRQn;
    return (IRQn_Type)(base + wiring->channel_idx - 1);
}

// --- transfer plumbing -------------------------------------------------------

/// Atomically claim transfer completion for either polling or interrupt code.
static bool adc_claim_completion(PHAL_ADC_Handle_t *handle) {
    uint32_t interrupt_state = __get_PRIMASK();
    __disable_irq();
    bool claimed = handle->busy;
    handle->busy = false;
    if (interrupt_state == 0U) {
        __enable_irq();
    }
    return claimed;
}

/// Stop the ADC and DMA channel, then clear the DMA status flags.
static void adc_teardown(PHAL_ADC_Handle_t *handle) {
    ADC_PRIV_stop_conversion(handle->config->instance);
    PHAL_DMA_stop(&handle->dma);
    PHAL_DMA_clearFlags(&handle->dma);
}

bool PHAL_ADC_init(PHAL_ADC_Handle_t *handle, const PHAL_ADC_Config_t *config) {
    if (handle == nullptr || !adc_config_is_valid(config)) {
        return false;
    }
    uint8_t index = adc_instance_index(config->instance);
    if (g_active_adc[index] != nullptr) {
        return false; // instance already claimed by another handle
    }

    if (!ADC_PRIV_configure(config->instance, config->channels, config->channel_count)) {
        return false;
    }
    if (!ADC_PRIV_enable(config->instance)) {
        return false;
    }

    handle->config         = config;
    handle->busy           = false;
    handle->transfer_error = false;
    handle->dma.wiring     = adc_dma_wiring(config->instance);
    handle->dma.params     = (PHAL_DMA_Params_t){
        .mem_addr   = 0U,
        .tx_size    = 0U,
        .priority   = DMA_PRIORITY_HIGH,
        .mode       = DMA_MODE_CIRCULAR,
        .mem_inc    = true,
        .tx_isr_en  = true, // transfer-complete interrupt drives the callback
    };

    if (!PHAL_DMA_initWithCallback(&handle->dma, adc_dma_callback, handle)) {
        ADC_PRIV_disable(config->instance);
        handle->config = nullptr;
        return false;
    }

    g_active_adc[index] = handle;
    return true;
}

bool PHAL_ADC_readDMA(PHAL_ADC_Handle_t *handle, uint16_t *buffer, uint16_t length) {
    if (handle == nullptr || handle->dma.channel == nullptr || buffer == nullptr) {
        return false;
    }
    if (handle->busy || length == 0U || ((uint32_t)buffer & 1U) != 0U) {
        return false;
    }

    // (Re)arm the DMA channel: fresh flags, new buffer/length, channel enabled
    handle->dma.params.tx_size = length;
    if (!PHAL_DMA_setMemAddress(&handle->dma, (uint32_t)buffer)
        || !PHAL_DMA_restart(&handle->dma)) {
        return false;
    }

    handle->busy           = true;
    handle->transfer_error = false;
    ADC_PRIV_prepare_transfer(handle->config->instance, true);
    ADC_PRIV_start_conversion(handle->config->instance);
    return true;
}

bool PHAL_ADC_readBlocking(PHAL_ADC_Handle_t *handle, uint16_t *buffer, uint16_t length, uint32_t timeout) {
    if (handle == nullptr || handle->config == nullptr || handle->dma.channel == nullptr) {
        return false;
    }

    // Keep completion in the caller's context. Any IRQ raised while polling is
    // cleared after teardown, before restoring the channel's prior NVIC state.
    IRQn_Type irqn = adc_dma_irqn(handle->config->instance);
    bool irq_was_enabled = NVIC_GetEnableIRQ(irqn) != 0U;
    NVIC_DisableIRQ(irqn);

    if (!PHAL_ADC_readDMA(handle, buffer, length)) {
        if (irq_was_enabled) {
            NVIC_EnableIRQ(irqn);
        }
        return false;
    }

    while (handle->busy && !PHAL_DMA_isComplete(&handle->dma)
           && !PHAL_DMA_isError(&handle->dma) && timeout > 0U) {
        timeout--;
    }

    bool claimed = adc_claim_completion(handle);
    bool dma_error = claimed && PHAL_DMA_isError(&handle->dma);
    bool complete = claimed && !dma_error && PHAL_DMA_isComplete(&handle->dma);
    if (claimed) {
        handle->transfer_error = dma_error;
        adc_teardown(handle);
    }

    NVIC_ClearPendingIRQ(irqn);
    if (irq_was_enabled) {
        NVIC_EnableIRQ(irqn);
    }

    if (complete) {
        PHAL_ADC_conversionCompleteCallback(handle);
    }
    return complete;
}

bool PHAL_ADC_stop(PHAL_ADC_Handle_t *handle) {
    if (handle == nullptr || handle->dma.channel == nullptr) {
        return false;
    }

    if (adc_claim_completion(handle)) {
        adc_teardown(handle);
        handle->transfer_error = false;
    }
    return true;
}

bool PHAL_ADC_busy(const PHAL_ADC_Handle_t *handle) {
    if (handle == nullptr) {
        return false;
    }
    return handle->busy;
}

// --- interrupt handling --------------------------------------------------------

/// Process DMA completion or error status for an ADC handle.
static void adc_dma_irq_handler(PHAL_ADC_Handle_t *handle) {
    if (handle == nullptr) {
        return;
    }

    if (PHAL_DMA_isError(&handle->dma)) {
        if (adc_claim_completion(handle)) {
            handle->transfer_error = true;
            adc_teardown(handle);
            PHAL_ADC_conversionCompleteCallback(handle);
        }
        return;
    }
    if (!PHAL_DMA_isComplete(&handle->dma)) {
        return;
    }

    if (handle->dma.params.mode == DMA_MODE_CIRCULAR) {
        // Circular transfers remain active and report each completed buffer.
        // Clear the status before invoking user code so the next wrap can
        // raise a fresh interrupt.
        PHAL_DMA_clearFlags(&handle->dma);
        handle->transfer_error = false;
        if (handle->busy) {
            PHAL_ADC_conversionCompleteCallback(handle);
        }
        return;
    }

    if (adc_claim_completion(handle)) {
        adc_teardown(handle);
        PHAL_ADC_conversionCompleteCallback(handle);
    }
}

static void adc_dma_callback(void *ctx) {
    adc_dma_irq_handler((PHAL_ADC_Handle_t *)ctx);
}

[[gnu::weak]] void PHAL_ADC_conversionCompleteCallback(PHAL_ADC_Handle_t *handle) {
    (void)handle;
}
