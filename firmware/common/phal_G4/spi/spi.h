/**
 * @file spi.h
 * @brief G4 SPI public API
 * @author Ronak Jain (jain717@purdue.edu)
 * @author Shriya Balu (balu@purdue.edu)
 */

#ifndef _PHAL_G4_SPI_H
#define _PHAL_G4_SPI_H

#include <stddef.h>

#include "common/phal_G4/dma/dma.h"

/**
 * @brief SPI operating mode.
 */
typedef enum {
    SPI_MODE_MASTER = 0,
    SPI_MODE_SLAVE  = 1,
} SPI_Mode;

/**
 * @brief SPI clock polarity.
 */
typedef enum {
    SPI_CPOL_IDLE_LOW  = 0,
    SPI_CPOL_IDLE_HIGH = 1,
} SPI_CPOL;

/**
 * @brief SPI clock phase.
 */
typedef enum {
    SPI_CPHA_FIRST_EDGE  = 0,
    SPI_CPHA_SECOND_EDGE = 1,
} SPI_CPHA;

/**
 * @brief SPI peripheral configuration.
 */
typedef struct {
    uint32_t data_rate;          /*!< Desired SPI clock frequency (Hz) */
    uint8_t data_len;            /*!< Transfer width in bits */
    SPI_Mode mode;               /*!< Master or slave mode */

    bool nss_sw;                 /*!< true = software-controlled chip select */
    GPIO_TypeDef *nss_gpio_port; /*!< Chip select GPIO port (master only) */
    uint32_t nss_gpio_pin;       /*!< Chip select GPIO pin (master only) */

    SPI_CPOL cpol;               /*!< Clock idle polarity */
    SPI_CPHA cpha;               /*!< Clock sampling phase */

    PHAL_DMA_Handle_t *rx_dma;   /*!< RX DMA handle (required, all transfers are DMA backed) */
    PHAL_DMA_Handle_t *tx_dma;   /*!< TX DMA handle (required, all transfers are DMA backed) */

    volatile bool _busy;                 /*!< Internal transfer busy flag */
    volatile bool _error;                /*!< Internal DMA transfer error flag */
    volatile bool _direct_mode_error;    /*!< Internal DMA direct mode error flag */
    volatile bool _fifo_overrun;         /*!< Internal DMA FIFO overrun flag */

    SPI_TypeDef *periph;         /*!< SPI peripheral instance */
} SPI_InitConfig_t;

/**
 * @brief Initialize an SPI peripheral for DMA-based transfers.
 *
 * Configures the SPI peripheral, initializes the associated DMA channels,
 * and prepares the peripheral for subsequent transfers.
 *
 * @param handle SPI configuration structure.
 * @return true if initialization was successful, false otherwise.
           Only can be false if DMA initialization fails.
 */
bool PHAL_SPI_init(SPI_InitConfig_t *handle);

/**
 * @brief Start a DMA-backed SPI transfer.
 *
 * Non-blocking: returns immediately after the DMA transfer has been started.
 * If another transfer is already in progress, block until it completes before
 * starting the new transfer.
 *
 * Passing NULL for either buffer transmits or receives dummy data.
 *
 * @param spi SPI configuration.
 * @param out_data Transmit buffer, or NULL to transmit dummy (zero) bytes.
 * @param in_data Receive buffer, or NULL to discard received bytes.
 * @param data_len Number of bytes to transfer (includes both TX and RX).
 */
void PHAL_SPI_transfer(SPI_InitConfig_t *spi,
                       const uint8_t *out_data,
                       uint8_t *in_data,
                       uint16_t data_len);

/**
 * @brief Perform a blocking DMA-backed SPI transfer. (Wrapper around PHAL_SPI_transfer.)
 *
 * Blocks until the SPI peripheral is available, starts the transfer,
 * and does not return until the transfer has completed.
 *
 * Passing NULL for either buffer transmits or receives dummy data.
 *
 * @param spi SPI configuration.
 * @param out_data Transmit buffer, or NULL to transmit dummy (zero) bytes.
 * @param in_data Receive buffer, or NULL to discard received bytes.
 * @param data_len Number of bytes to transfer (includes both TX and RX).
 */
void PHAL_SPI_transferBlocking(SPI_InitConfig_t *spi,
                               const uint8_t *out_data,
                               uint8_t *in_data,
                               uint16_t data_len);

/**
 * @brief Check whether the SPI peripheral is busy.
 *
 * @param cfg SPI configuration.
 * @return true if a transfer is currently in progress.
 */
bool PHAL_SPI_busy(const SPI_InitConfig_t *cfg);

/**
 * @brief Weak callback fired when a DMA-backed SPI transfer completes.
 *
 * Called after the transfer has completed and the SPI handle has been
 * marked idle.
 *
 * Default implementation does nothing.
 *
 * @param spi SPI configuration that completed the transfer.
 */
extern void PHAL_SPI_txCallback(SPI_InitConfig_t *spi);

#endif /* _PHAL_G4_SPI_H */
