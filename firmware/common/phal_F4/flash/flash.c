/**
 * @file flash.c
 * @author Chris McGalliard - port of L4 by Adam Busch (busch8@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2023-08-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "common/phal_F4/flash/flash.h"

/**
 * @brief Convert address to sector number
 *        Assumes addr is within flash addr space
 *
 * @param addr Flash address
 * @return The flash sector
 */
static uint32_t flashGetSector(uint32_t addr) {
    uint32_t sector = 0;
    if (addr < ADDR_FLASH_SECTOR_1)
        sector = 0;
    else if (addr < ADDR_FLASH_SECTOR_2)
        sector = 1;
    else if (addr < ADDR_FLASH_SECTOR_3)
        sector = 2;
    else if (addr < ADDR_FLASH_SECTOR_4)
        sector = 3;
    else if (addr < ADDR_FLASH_SECTOR_5)
        sector = 4;
    else if (addr < ADDR_FLASH_SECTOR_6)
        sector = 5;
    else if (addr < ADDR_FLASH_SECTOR_7)
        sector = 6;
#ifdef STM32F407xx
    else if (addr < ADDR_FLASH_SECTOR_8)
        sector = 7;
    else if (addr < ADDR_FLASH_SECTOR_9)
        sector = 8;
    else if (addr < ADDR_FLASH_SECTOR_10)
        sector = 9;
    else if (addr < ADDR_FLASH_SECTOR_11)
        sector = 10;
    else
        sector = 11;
#else
    sector = 7;
#endif
    return sector;
}

static uint8_t flashUnlock() {
    uint32_t timeout = 0;

    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT)
        return FLASH_TIMEOUT;

    FLASH->KEYR = FLASH_KEY_1; // Keys must be set to unlock flash
    FLASH->KEYR = FLASH_KEY_2;
    __DSB(); // Data synchonization barrier (wait for any memory instructions to finish)
    return FLASH_OK;
}

static void flashLock() {
    __DSB();
    FLASH->CR |= FLASH_CR_LOCK; // Set-only bit, will be cleared upon next succcessful flash unlock
}

uint32_t PHAL_flashReadU32(uint32_t addr) {
    // technically it's undef bheavior if we read flash while writing but this is slow
#if 0
    uint32_t val = 0xFFFFFFFF;
    uint8_t ret;
    if (addr < FLASH_BASE || addr > FLASH_END)
        return 0xFFFFFFFF;

    ret = flashUnlock();
    if (ret != FLASH_OK) return ret;

    val = *((__IO uint32_t*) addr);

    flashLock();
#else
    uint32_t val = *((__IO uint32_t*)addr);
#endif
    return val;
}

uint8_t PHAL_flashWriteU32(uint32_t Address, uint32_t value) {
    uint32_t timeout = 0;
    uint8_t ret;
    ret = flashUnlock();
    if (ret != FLASH_OK)
        return ret;

    // Ensure word has been cleared
    if (*(uint32_t*)Address != 0xFFFFFFFF)
        return FLASH_ADDR_NOT_CLEARED;

    // Wait until not busy
    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT)
        return FLASH_TIMEOUT;
    timeout = 0;

    // Set 32 bit parallelism (see RM 0090 Page 85)
    FLASH->CR &= ~(FLASH_CR_PSIZE_Msk);
    FLASH->CR |= FLASH_CR_PSIZE_1;

    // Enable flash programming
    FLASH->CR |= FLASH_CR_PG;

    // Write word
    *(__IO uint32_t*)Address = value;
    __DSB();

    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT)
        return FLASH_TIMEOUT;

    // Disable flash programming
    FLASH->CR &= ~(FLASH_CR_PG);
    flashLock();

    // Check for proper flash write
    if (*((uint32_t*)Address) != value) {
        return FLASH_FAIL;
    }

    return FLASH_OK;
}

uint8_t PHAL_flashWriteU64(uint32_t Address, uint64_t Data) {
    uint8_t ret;
    ret = PHAL_flashWriteU32(Address, (uint32_t)Data);
    if (ret != FLASH_OK)
        return ret;
    ret = PHAL_flashWriteU32(Address + 4, (uint32_t)(Data >> 32));
    return ret;
}

uint8_t PHAL_flashWriteU32_Buffered(uint32_t Address, uint32_t* data, uint32_t count) {
    uint32_t timeout = 0;
    uint8_t ret;
    ret = flashUnlock();
    if (ret != FLASH_OK)
        return ret;

    // Wait until not busy
    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT)
        return FLASH_TIMEOUT;
    timeout = 0;

    // Set 32 bit parallelism (see RM 0090 Page 85)
    FLASH->CR &= ~(FLASH_CR_PSIZE_Msk);
    FLASH->CR |= FLASH_CR_PSIZE_1;

    // Enable flash programming
    FLASH->CR |= FLASH_CR_PG;

    for (uint32_t i = 0; i < count; i++)
        *(__IO uint32_t*)(Address + i * 4) = data[i];
    __DSB();

    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT)
        return FLASH_TIMEOUT;

    // Disable flash programming
    FLASH->CR &= ~(FLASH_CR_PG);
    flashLock();

    return FLASH_OK;
}

uint8_t PHAL_flashErasePage(uint8_t page) {
    // Validate page request
    if (page > MAX_FLASH_SECTOR) {
        return FLASH_FAIL;
    }

    uint32_t timeout = 0;
    uint8_t ret;
    ret = flashUnlock();
    if (ret != FLASH_OK)
        return ret;

    // Enable sector erase
    FLASH->CR |= FLASH_CR_SER;

    // Set the sector to be erased (0-11)
    FLASH->CR &= ~FLASH_CR_SNB_Msk;
    FLASH->CR |= ((page) << FLASH_CR_SNB_Pos) & FLASH_CR_SNB_Msk;

    // Execute the erase
    FLASH->CR |= FLASH_CR_STRT;

    while ((FLASH->SR & FLASH_SR_BSY) && ++timeout < PHAL_FLASH_TIMEOUT)
        __asm__("nop");
    if (timeout == PHAL_FLASH_TIMEOUT) {
        flashLock();
        return FLASH_TIMEOUT;
    }

    // Disable page erase and clear the page request
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB_Msk);

    flashLock();

    return FLASH_OK;
}

uint8_t PHAL_flashErase(uint32_t* addr, uint32_t words) {
    uint8_t ret;
    if (words == 0)
        return FLASH_OK;
    uint32_t* end_addr_inclusive = addr + (words - 1);
    if ((uint32_t)addr < FLASH_BASE || (uint32_t)end_addr_inclusive > (FLASH_END - 3))
        return FLASH_FAIL;
    uint8_t sector_start = flashGetSector((uint32_t)addr);
    uint8_t sector_end   = flashGetSector((uint32_t)end_addr_inclusive);
    for (uint8_t i = sector_start; i <= sector_end; ++i) {
        ret = PHAL_flashErasePage(i);
        if (ret != FLASH_OK)
            return ret;
    }
    return FLASH_OK;
}
