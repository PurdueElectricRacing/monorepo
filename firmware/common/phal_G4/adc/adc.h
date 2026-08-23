/**
 * @file adc.h
 * @brief G4 ADC public API. DMA-only, DMA-first conversions.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#ifndef PHAL_G4_ADC_H
#define PHAL_G4_ADC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"

#include "common/phal_G4/dma/dma.h"

/// Maximum number of channels one ADC conversion sequence can hold
static constexpr uint8_t PHAL_ADC_MAX_CHANNEL_COUNT = 16U;

/// Highest ADC channel number (IN18 on the G474)
static constexpr uint8_t PHAL_ADC_MAX_CHANNEL_NUMBER = 18U;

/**
 * @brief One entry in the conversion sequence
 *
 * Channels are converted in array order (channels[0] first). The sampling
 * time is fixed to the maximum (640.5 cycles) for every channel.
 */
typedef struct {
    uint8_t channel; /*!< ADC channel number, 1 - 18 (IN1..IN18) */
} PHAL_ADC_ChannelConfig_t;

/**
 * @brief Static configuration for one ADC instance
 *
 * Everything else is fixed by the driver:
 * - 12-bit resolution, right-aligned data
 * - continuous conversion engine with circular DMA (each transfer keeps
 *   refreshing the destination buffer)
 * - ADC clock = HCLK / 4 (4 MHz @ 16 MHz HCLK, 42.5 MHz @ 170 MHz HCLK)
 * - no oversampling, no hardware trigger (software start only)
 */
typedef struct {
    ADC_TypeDef *instance;               /*!< ADC1, ADC2, ADC3 or ADC4 */
    const PHAL_ADC_ChannelConfig_t *channels; /*!< sequence, converted in array order */
    size_t channel_count;                /*!< entries in channels[], 1 - 16 */
} PHAL_ADC_Config_t;

/**
 * @brief Runtime handle for one ADC instance
 *
 * Zero-initialize (or `static` declare) and pass to PHAL_ADC_init().
 * Do not touch the fields yourself.
 */
typedef struct {
    const PHAL_ADC_Config_t *config; /*!< the config passed to PHAL_ADC_init() */
    PHAL_DMA_Handle_t dma;           /*!< DMA channel claimed for this ADC */
    volatile bool busy;              /*!< true while a transfer is in flight */
    volatile bool transfer_error;    /*!< set if the last in-flight transfer hit a DMA error */
} PHAL_ADC_Handle_t;

/**
 * @brief Configure an ADC instance and its DMA channel
 *
 * What this does, in order:
 * 1. Enables the ADC (and DMA) clocks
 * 2. Runs the ADC hardware calibration
 * 3. Configures the conversion sequence, sample times, and fixed conversion
 *    options (12-bit, right-aligned, continuous, DMA-enabled, no oversampling)
 * 4. Enables the ADC and waits until it reports ready
 * 5. Claims the ADC's dedicated DMA channel and enables its interrupt
 *
 * The DMA channel is fixed per instance (ADC1 -> DMA1 ch1, ADC2 -> DMA2 ch1,
 * ADC3 -> DMA2 ch2, ADC4 -> DMA2 ch3). Only one handle may claim each ADC
 * instance; handles remain claimed for the lifetime of the application.
 *
 * @param handle zero-initialized handle to populate
 * @param config static configuration to use; must stay valid for the
 *        lifetime of the handle
 * @return true on success, false if the config is invalid, the instance is
 *         already claimed, calibration fails, or the DMA channel is taken
 * @note Initialization configures the ADC and DMA but does not start sampling;
 *       call PHAL_ADC_readDMA() once after initialization to start background
 *       acquisition.
 */
bool PHAL_ADC_init(PHAL_ADC_Handle_t *handle, const PHAL_ADC_Config_t *config);

/**
 * @brief Start an asynchronous circular-DMA conversion into `buffer`
 *
 * The ADC continuously refreshes `buffer` and remains active until
 * PHAL_ADC_stop() is called. `PHAL_ADC_conversionCompleteCallback()` fires
 * (from the DMA interrupt context) after each buffer wrap.
 *
 * If `length` is not a multiple of the sequence length, the final burst is a
 * partial sequence (still one sample per conversion).
 *
 * Fails without starting anything if a transfer is already in flight, the
 * handle was never initialized, `buffer` is null or not halfword-aligned, or
 * `length` is zero.
 *
 * @param handle initialized handle
 * @param buffer destination, must stay valid until PHAL_ADC_stop() is called
 * @param length number of 16-bit samples to collect (1 - 65535)
 * @return true if the transfer was started, false otherwise
 * @note Only one background transfer may be active for a handle.
 */
bool PHAL_ADC_readDMA(PHAL_ADC_Handle_t *handle, uint16_t *buffer, uint16_t length);

/**
 * @brief Synchronous one-buffer conversion using the DMA engine
 *
 * Starts the circular acquisition, blocks until one buffer has completed, then
 * stops the acquisition. A successful transfer fires
 * `PHAL_ADC_conversionCompleteCallback()` before returning.
 *
 * Note: `timeout` is a count of busy-wait iterations, not a wall-clock time.
 * At 16 MHz one iteration is a handful of CPU cycles; the ADC itself takes
 * roughly (640.5 + 12.5) / fADC seconds per sample (about 163 us at 4 MHz fADC).
 *
 * @param handle initialized handle
 * @param buffer destination for the samples
 * @param length number of 16-bit samples to collect
 * @param timeout busy-wait iteration budget (0 = return immediately)
 * @return true if all samples were collected, false on timeout or DMA error
 */
bool PHAL_ADC_readBlocking(PHAL_ADC_Handle_t *handle, uint16_t *buffer, uint16_t length, uint32_t timeout);

/**
 * @brief Abort any transfer in flight and leave the ADC idle
 *
 * Safe to call at any time, even with no transfer running (it is then a no-op).
 * Does not fire the completion callback.
 *
 * @param handle initialized handle
 * @return true if the handle is initialized, false otherwise
 */
bool PHAL_ADC_stop(PHAL_ADC_Handle_t *handle);

/**
 * @brief Check whether a transfer is currently in flight
 * @param handle initialized handle
 * @return true while a PHAL_ADC_readDMA() transfer is running
 */
bool PHAL_ADC_busy(const PHAL_ADC_Handle_t *handle);

/**
 * @brief Weak callback fired once per completed DMA buffer
 *
 * Called from the DMA interrupt context after each asynchronous circular-DMA
 * buffer completes, and from the caller's context at the end of a successful
 * PHAL_ADC_readBlocking(). On a DMA error, `handle->transfer_error` is true.
 * Keep it short; the default implementation does nothing.
 *
 * @param handle the handle whose transfer just completed
 */
extern void PHAL_ADC_conversionCompleteCallback(PHAL_ADC_Handle_t *handle);

#endif // PHAL_G4_ADC_H
