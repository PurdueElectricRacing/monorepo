/**
 * @file crc.h
 * @brief STM32G4 CRC public API.
 * @author Hannah Song (song892@purdue.edu)
 */

#ifndef __PHAL_G4_CRC_H__
#define __PHAL_G4_CRC_H__

#include "stm32g474xx.h"

/**
 * @brief Initialize and configure the hardware CRC peripheral.
 *
 * Enables the CRC peripheral clock on AHB1, then explicitly configures the
 * unit for CRC-32/MPEG-2: 32-bit polynomial 0x04C11DB7, initial value
 * 0xFFFFFFFF, no input or output bit reversal, and no final XOR. These
 * parameters are written explicitly rather than relying on the peripheral's
 * reset defaults.
 *
 * Assumes the system and AHB clocks are already configured. Must be called
 * once before any call to PHAL_CRC_calculate().
 */
void PHAL_CRC_init(void);

/**
 * @brief Compute a CRC-32/MPEG-2 over a word buffer using the hardware unit.
 *
 * Resets the CRC unit (reloading the initial value), feeds each 32-bit word
 * into the peripheral in order, and returns the accumulated remainder. Input
 * is consumed as 32-bit words fed most-significant-bit first; no reflection
 * or final XOR is applied.
 *
 * Assumes PHAL_CRC_init() has already been called. Uses the single shared
 * CRC peripheral, so it is not reentrant and must not be called concurrently
 * from another context.
 *
 * @param data  Pointer to the buffer of 32-bit words to process.
 * @param words Number of 32-bit words in @p data.
 * @return The 32-bit CRC-32/MPEG-2 remainder over the input.
 */
uint32_t PHAL_CRC_calculate(const uint32_t *data, uint32_t words);

/**
 * @brief Software reference implementation of PHAL_CRC_calculate().
 *
 * Computes the same CRC-32/MPEG-2 remainder as PHAL_CRC_calculate() using a
 * nibble lookup table, producing bit-identical results. Independent of the
 * CRC peripheral: does not require PHAL_CRC_init() and touches no hardware
 * registers. Intended as a cross-check against the hardware path, or as a
 * fallback where the peripheral is unavailable.
 *
 * @param data  Pointer to the buffer of 32-bit words to process.
 * @param words Number of 32-bit words in @p data.
 * @return The 32-bit CRC-32/MPEG-2 remainder over the input.
 */
uint32_t PHAL_CRC_calculateSw(const uint32_t *data, uint32_t words);

#endif // __PHAL_G4_CRC_H__
