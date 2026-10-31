/**
 * @file flash.h
 * @author Chris McGalliard - port of L4 by Adam Busch (busch8@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2023-08-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _PHAL_FLASH_H_
#define _PHAL_FLASH_H_

#include "common/phal_F4/phal_F4.h"

enum {
    FLASH_OK = 0,
    FLASH_FAIL,
    FLASH_ADDR_NOT_CLEARED,
    FLASH_TIMEOUT,
};

// Flash magic numbers obtained from family reference manual
#define FLASH_KEY_1 0x45670123
#define FLASH_KEY_2 0xCDEF89AB

#define PHAL_FLASH_TIMEOUT 500000U

/* Base address of the Flash sectors */
#define ADDR_FLASH_SECTOR_0  ((uint32_t)0x08000000) /* Base @ of Sector 0, 16 Kbyte */
#define ADDR_FLASH_SECTOR_1  ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbyte */
#define ADDR_FLASH_SECTOR_2  ((uint32_t)0x08008000) /* Base @ of Sector 2, 16 Kbyte */
#define ADDR_FLASH_SECTOR_3  ((uint32_t)0x0800C000) /* Base @ of Sector 3, 16 Kbyte */
#define ADDR_FLASH_SECTOR_4  ((uint32_t)0x08010000) /* Base @ of Sector 4, 64 Kbyte */
#define ADDR_FLASH_SECTOR_5  ((uint32_t)0x08020000) /* Base @ of Sector 5, 128 Kbyte */
#define ADDR_FLASH_SECTOR_6  ((uint32_t)0x08040000) /* Base @ of Sector 6, 128 Kbyte */
#define ADDR_FLASH_SECTOR_7  ((uint32_t)0x08060000) /* Base @ of Sector 7, 128 Kbyte */
#define ADDR_FLASH_SECTOR_8  ((uint32_t)0x08080000) /* Base @ of Sector 8, 128 Kbyte */
#define ADDR_FLASH_SECTOR_9  ((uint32_t)0x080A0000) /* Base @ of Sector 9, 128 Kbyte */
#define ADDR_FLASH_SECTOR_10 ((uint32_t)0x080C0000) /* Base @ of Sector 10, 128 Kbyte */
#define ADDR_FLASH_SECTOR_11 ((uint32_t)0x080E0000) /* Base @ of Sector 11, 128 Kbyte */

#define ADDR_FLASH_SECTOR_12 ((uint32_t)0x08100000) /* 128 KiB */
#define ADDR_FLASH_SECTOR_13 ((uint32_t)0x08120000) /* 128 KiB */
#define ADDR_FLASH_SECTOR_14 ((uint32_t)0x08140000) /* 128 KiB */
#define ADDR_FLASH_SECTOR_15 ((uint32_t)0x08160000) /* 128 KiB */

#define USER_FLASH_END_ADDRESS ((uint32_t)0x0817FFFF) /* Last usable flash address */
#define MAX_FLASH_SECTOR       (16 - 1)

uint32_t PHAL_flashReadU32(uint32_t addr);

uint8_t PHAL_flashWriteU32(uint32_t address, uint32_t value);
uint8_t PHAL_flashWriteU32_Buffered(uint32_t Address, uint32_t* data, uint32_t count);

uint8_t PHAL_flashWriteU64(uint32_t address, uint64_t data);
uint8_t PHAL_flashErasePage(uint8_t page);
uint8_t PHAL_flashErase(uint32_t* addr, uint32_t words);

#endif