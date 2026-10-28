/**
 * @file spi_baud.c
 * @brief G4 SPI baud-rate prescaler selection (pure math, no register access)
 */

#include "spi_baud.h"

bool PHAL_SPI_selectBaudRatePrescaler(uint32_t bus_clock_hz, uint32_t data_rate, uint32_t *br) {
    if (data_rate == 0U) {
        return false;
    }

    // Try the fastest divider first. SCK = bus / 2^(BR+1) <= data_rate is
    // checked as bus <= data_rate * 2^(BR+1), in 64 bits, so integer
    // truncation can never let SCK exceed data_rate
    for (uint32_t candidate = 0U; candidate <= PHAL_SPI_BAUD_BR_MAX; candidate++) {
        if ((uint64_t)bus_clock_hz <= ((uint64_t)data_rate << (candidate + 1U))) {
            *br = candidate;
            return true;
        }
    }
    return false;
}
