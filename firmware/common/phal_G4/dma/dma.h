/**
 * @file dma.h
 * @brief G4 DMA Peripheral public API implementation
 * @author Shriya Balu (balu@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PHAL_G4_DMA_H
#define PHAL_G4_DMA_H

#include <stdint.h>

#include "common/phal_G4/phal_G4.h"

#include "common/phal_G4/dma/dma_wiring.h"

/// Relative priority when multiple DMA requests are pending at once
typedef enum {
    DMA_PRIORITY_LOW       = 0,
    DMA_PRIORITY_MEDIUM    = 1,
    DMA_PRIORITY_HIGH      = 2,
    DMA_PRIORITY_VERY_HIGH = 3,
} PHAL_DMA_Priority_t;

/// What a channel does once its transfer count reaches zero
typedef enum {
    DMA_MODE_NORMAL   = 0, /**< Stops (call PHAL_DMA_restart() to re-run) */
    DMA_MODE_CIRCULAR = 1, /**< Automatically reload the count and restart */
    DMA_MODE_MEM2MEM  = 2, /**< Copy memory-to-memory (no DMAMUX request involved) */
} PHAL_DMA_Mode_t;

/**
 * @brief Per-transfer parameters (user can configure)
 */
typedef struct {
    uint32_t mem_addr;         /*!< Memory buffer address, second memory address for MEM2MEM */
    uint16_t tx_size;          /*!< Number of data elements to transfer */
    PHAL_DMA_Priority_t priority;
    PHAL_DMA_Mode_t mode;
    bool mem_inc;              /*!< Increment memory address after each transfer */
    bool tx_isr_en;            /*!< Enable transfer-complete and transfer-error interrupts */
} PHAL_DMA_Params_t;


/**
 * Invoked from the DMA PHAL's own ISR when this handle's channel fires.
 * Runs in interrupt context. ctx is whatever was passed to PHAL_DMA_registerCallback.
 * ctx must remain valid for as long as the channel could still raise an interrupt naming this handle
 */
typedef void (*PHAL_DMA_IRQCallbackFn_t)(void *ctx);

/**
 * @brief Optional callback for DMA interrupts
 *
 * If the user wants to handle DMA interrupts, they can register a callback function and context pointer.
 * The callback will be invoked from the DMA PHAL's own ISR when this handle's channel fires.
 * The context pointer must remain valid for as long as the channel could still raise an interrupt naming this handle.
 */
typedef struct {
    PHAL_DMA_IRQCallbackFn_t irq_fn;
    void *ctx;
} PHAL_DMA_Callback_t;

/**
 * @brief A configured DMA transfer: fixed wiring + chosen parameters
 *
 * channel is populated by PHAL_DMA_init(), do not specify when constructing PHAL_DMA_Handle_t.
 */
typedef struct {
    const PHAL_DMA_Wiring_t *wiring;
    PHAL_DMA_Params_t params;
    DMA_Channel_TypeDef *channel; /**< Populated by PHAL_DMA_init()!! */
    PHAL_DMA_Callback_t callback; /**< Optional callback for DMA interupts.
                                       Populated by PHAL_DMA_registerCallback() */
} PHAL_DMA_Handle_t;


/**
 * @brief Register a callback for this handle's channel interrupt.
 *
 * Call before PHAL_DMA_init. The DMA PHAL owns every DMAx_ChannelN_IRQHandler
 * vector; this is how another HAL gets notified instead of defining (and
 * fighting over) that vector itself. PHAL_DMA_init also arms the NVIC line
 * for this channel automatically once a callback is registered.
 */
bool PHAL_DMA_initWithCallback(PHAL_DMA_Handle_t *handle, PHAL_DMA_IRQCallbackFn_t irq_fn, void *ctx);

/**
 * @brief Claim a DMA channel and configure it from handle->wiring and handle->params
 
 * Does not start the transfer (call PHAL_DMA_start() afterward).
 *
 * @param handle wiring + params to configure. handle->channel is populated in on success
 * @return true on success; false if handle/wiring invalid (NULL, channel_idx
 * is outside 1-8, periph isn't DMA1/DMA2, or that periph/channel_idx
 * is already claimed by another live handle)
 */
bool PHAL_DMA_init(PHAL_DMA_Handle_t *handle);

/**
 * @brief Disable the channel and release its claim so another handle can
 * use the periph/channel_idx afterward
 * @return true on success, false if handle was never successfully init-ed
 */
bool PHAL_DMA_deinit(PHAL_DMA_Handle_t *handle);

/**
 * @brief Enable the channel, starting the transfer configured by PHAL_DMA_init()
 * @return true on success, false if handle was never successfully init-ed
 */
bool PHAL_DMA_start(PHAL_DMA_Handle_t *handle);

/**
 * @brief Disable the channel, stopping the transfer immediately
 * 
 * Whatever hasn't transferred yet is left un-transferred
 * 
 * @return true on success, false if handle was never successfully init-ed
 */
bool PHAL_DMA_stop(PHAL_DMA_Handle_t *handle);

/**
 * @brief Reload the transfer count from handle->params.tx_size and start again
 * 
 * Safe to call regardless of whether the channel state (enabled, mid-transfer, or stopped on an error),
 * it always disables first.
 *
 * @return true on success, false if handle was never successfully init-ed
 */
bool PHAL_DMA_restart(PHAL_DMA_Handle_t *handle);

/**
 * @brief Change the memory address for the next transfer
 * @return true on success, false if handle was never successfully
 * init-ed or the channel is currently enabled
 */
bool PHAL_DMA_setMemAddress(PHAL_DMA_Handle_t *handle, uint32_t address);

/**
 * @brief Change the transfer length (element count) for the next transfer
 * @return true on success, false if handle was never successfully
 * init-ed, or the channel is currently enabled
 */
bool PHAL_DMA_setLength(PHAL_DMA_Handle_t *handle, uint16_t length);

/**
 * @brief Number of elements still left to transfer on this channel
 * @return elements outstanding, or 0 if handle was never successfully init-ed
 */
uint16_t PHAL_DMA_getRemaining(PHAL_DMA_Handle_t *handle);

/**
 * @brief Check whether the channel is currently enabled (transfer in
 * progress, or in circular mode, running continuously)
 *
 * @return true if the channel is enabled, false if disabled or handle
 * was never successfully init-ed
 */
bool PHAL_DMA_isBusy(PHAL_DMA_Handle_t *handle);

/**
 * @brief Check whether the channel has finished its transfer (TC flag set)
 *
 * A normal-mode channel clears EN and latches this flag on its own when the
 * transfer count reaches zero, so this is the flag a blocking wrapper should
 * busy-wait on.
 *
 * @param handle initialized DMA handle
 * @return true if the transfer-complete flag is set, false otherwise
 */
bool PHAL_DMA_isComplete(PHAL_DMA_Handle_t *handle);

/**
 * @brief Check whether the channel reported a transfer error (TE flag set)
 *
 * A bus error during the transfer latches this flag and stops the channel.
 *
 * @param handle initialized DMA handle
 * @return true if the transfer-error flag is set, false otherwise
 */
bool PHAL_DMA_isError(PHAL_DMA_Handle_t *handle);

/**
 * @brief Clear every latched status flag (complete/error/global) for the
 * channel, so a finished transfer can be reused
 *
 * @param handle initialized DMA handle
 * @return true on success, false if handle was never successfully initialized
 */
bool PHAL_DMA_clearFlags(PHAL_DMA_Handle_t *handle);

/**
 * @brief Get the DMA peripheral (DMA1 or DMA2) for a given handle
 * 
 * @return Return nullptr if handle is null otherwise return the DMA peripheral for the given handle
 */
DMA_TypeDef *PHAL_DMA_getPeriph(PHAL_DMA_Handle_t *handle);

/**
 * @brief Get the channel number (1-8) for a given handle
 * 
 * @return uint8_t Return 0 if handle is null otherwise return the channel number for the given handle
 */
uint8_t PHAL_DMA_getChannelIdx(PHAL_DMA_Handle_t *handle);

/**
 * @brief Set whether the memory address increments after each transfer and rebuild the channel configuration
 *
 * If the channel is currently enabled, this function will not change the configuration
 * 
 * @param mem_inc true to increment memory address after each transfer, false to keep it constant
 */
void PHAL_DMA_setMemInc(PHAL_DMA_Handle_t *handle, bool mem_inc);


#endif // PHAL_G4_DMA_H