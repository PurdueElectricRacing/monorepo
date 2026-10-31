/**
 * @file crc.c
 * @author Eileen Yoon (eyn@purdue.edu)
 * @brief Hardware CRC32 w/ software fallback
 * @version 0.1
 * @date 2024-11-25
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "crc.h"

void PHAL_CRC32_Reset(void) {
    // CRC initializaion
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN; // F4 only supports CRC-32b
    CRC->CR = CRC_CR_RESET;
}

void PHAL_CRC32_Init(void) {
    PHAL_CRC32_Reset();
}

uint32_t PHAL_CRC32_Calculate(uint32_t* data, uint32_t count) {
    PHAL_CRC32_Reset();
    __DSB();

    for (uint32_t i = 0; i < count; i++)
        CRC->DR = data[i];

    return CRC->DR;
}

static const uint32_t crc32b_LUT[16] = {
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

static inline uint32_t update_crc(uint32_t crc, uint32_t data) {
    crc = crc ^ data; // Apply all 32-bits

    // Process 32-bits, 4 at a time, or 8 rounds
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];
    crc = (crc << 4) ^ crc32b_LUT[crc >> 28];

    return crc;
}

uint32_t PHAL_CRC32_CalculateSW(uint32_t* data, uint32_t count) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < count; i++)
        crc = update_crc(crc, data[i]);
    return crc;
}
