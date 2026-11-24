/**
 * @file crc_priv.c
 * @brief STM32G4 CRC register-level implementation.
 * @author Hannah Song (song892@purdue.edu)
 */

#include "phal_G4/crc/crc_priv.h"

void CRC_PRIV_enableClock(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
    // Dummy readback of the enable register: after the clock-enable write the
    // peripheral is not immediately reachable, so read the register back to
    // force the write to complete and stall until the CRC clock is running
    // before any CRC register is accessed.
    (void) RCC->AHB1ENR;
}

void CRC_PRIV_setConfig(void) {
    CRC->CR = 0;
    CRC->POL = CRC_PRIV_POLY_CRC32;
    CRC->INIT = CRC_PRIV_INIT_VALUE;
}

void CRC_PRIV_reset(void) {
    CRC->CR |= CRC_CR_RESET;
    __DSB();
}

void CRC_PRIV_feedWord(uint32_t word) {
    CRC->DR = word;
}

uint32_t CRC_PRIV_readResult(void) {
    return CRC->DR;
}