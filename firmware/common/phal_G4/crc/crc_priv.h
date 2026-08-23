/**
 * @file crc_priv.h
 * @brief STM32G4 CRC register-level implementation interface.
 * @author Hannah Song (song892@purdue.edu)
 */

#ifndef __PHAL_G4_CRC_PRIV_H__
#define __PHAL_G4_CRC_PRIV_H__

#include "stm32g474xx.h"

/// CRC-32/MPEG-2 polynomial.
static constexpr uint32_t CRC_PRIV_POLY_CRC32 = 0x04C11DB7;

/// Initial CRC value loaded into the data register on reset.
static constexpr uint32_t CRC_PRIV_INIT_VALUE = 0xFFFFFFFF;

/// Enable the CRC peripheral clock on AHB1.
void CRC_PRIV_enableClock(void);

/// Configure the polynomial, size, and initial value for CRC-32/MPEG-2.
void CRC_PRIV_setConfig(void);

/// Reset the CRC unit, reloading the initial value into the data register.
void CRC_PRIV_reset(void);

/// Feed one 32-bit word into the CRC data register.
void CRC_PRIV_feedWord(uint32_t word);

/// Read the accumulated CRC result from the data register.
uint32_t CRC_PRIV_readResult(void);

#endif // __PHAL_G4_CRC_PRIV_H__