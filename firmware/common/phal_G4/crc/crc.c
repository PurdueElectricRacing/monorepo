/**
 * @file crc.c
 * @brief STM32G4 CRC public-layer implementation.
 * @author Hannah Song (song892@purdue.edu)
 */

#include "phal_G4/crc/crc.h"
#include "phal_G4/crc/crc_priv.h"

void PHAL_CRC_init(void) {
    CRC_PRIV_enableClock();
    CRC_PRIV_setConfig();
}

uint32_t PHAL_CRC_calculate(const uint32_t *data, uint32_t words) {
    CRC_PRIV_reset();

    for (uint32_t i = 0; i < words; i++) {
        CRC_PRIV_feedWord(data[i]);
    }

    return CRC_PRIV_readResult();
}

/// Lookup table for the CRC-32/MPEG-2 polynomial 0x04C11DB7.
static const uint32_t crc32_LUT[16] = {
    0x00000000,
    0x04C11DB7,
    0x09823B6E,
    0x0D4326D9,
    0x130476DC,
    0x17C56B6B,
    0x1A864DB2,
    0x1E475005,
    0x2608EDB8,
    0x22C9F00F,
    0x2F8AD6D6,
    0x2B4BCB61,
    0x350C9B64,
    0x31CD86D3,
    0x3C8EA00A,
    0x384FBDBD,
};

/// Fold one 32-bit word into the running CRC, four bits at a time, MSB-first.
static uint32_t crc_step(uint32_t crc, uint32_t data) {
    crc ^= data;

    for (int i = 0; i < 8; i++) {
        crc = (crc << 4) ^ crc32_LUT[crc >> 28];
    }
    return crc;
}

uint32_t PHAL_CRC_calculateSw(const uint32_t *data, uint32_t words) {
    uint32_t crc = CRC_PRIV_INIT_VALUE;

    for (uint32_t i = 0; i < words; i++) {
        crc = crc_step(crc, data[i]);
    }
    return crc;
}