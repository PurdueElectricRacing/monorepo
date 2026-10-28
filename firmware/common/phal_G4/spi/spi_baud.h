/**
 * @file spi_baud.h
 * @brief G4 SPI baud-rate prescaler selection (pure math, no register access)
 */

#ifndef PHAL_G4_SPI_BAUD_H
#define PHAL_G4_SPI_BAUD_H

#include <stdbool.h>
#include <stdint.h>

/// Largest SPI_CR1.BR value. BR = n divides the bus clock by 2^(n + 1), so 2..256
static constexpr uint32_t PHAL_SPI_BAUD_BR_MAX = 7U;

/**
 * @brief Pick the SPI_CR1.BR value giving the fastest SCK that does not exceed data_rate
 *
 * Never rounds toward a faster clock than requested: data_rate is treated as
 * the device's maximum SCK frequency.
 *
 * @param bus_clock_hz APB clock feeding the SPI peripheral
 * @param data_rate maximum allowed SCK frequency in Hz
 * @param[out] br selected BR value (0 - PHAL_SPI_BAUD_BR_MAX), written only on success
 * @return false if data_rate is 0 or slower than bus_clock_hz / 256 (unreachable)
 */
bool PHAL_SPI_selectBaudRatePrescaler(uint32_t bus_clock_hz, uint32_t data_rate, uint32_t *br);

#endif // PHAL_G4_SPI_BAUD_H
