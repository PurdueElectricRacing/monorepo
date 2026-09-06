/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h" /* Declarations of disk functions */

#include "common/phal/rtc.h"

/* Set in defines.h file if you want it */
#ifndef TM_FATFS_CUSTOM_FATTIME
#define TM_FATFS_CUSTOM_FATTIME 0
#endif

#include <string.h>

#include "main.h"
#include "common/sdio/sdio.h"

/* Definitions of physical drive number for each media */
#define ATA       0
#define USB       1
#define SDRAM     2
#define SPI_FLASH 3

#define BLOCK_SIZE (512)

static volatile DSTATUS SD_SDIO_Stat = STA_NOINIT; /* Physical drive status */

static DRESULT _sdio_disk_read(BYTE* buff, DWORD sector, UINT count);
static DRESULT _sdio_disk_write(const BYTE* buff, DWORD sector, UINT count);

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(
    BYTE pdrv /* Physical drive nmuber (0..) */
) {
    // TODO: possible to ignore pdrv?

    /* Return low level status */
    if (PHAL_SDIO_init()) {
        SD_SDIO_Stat &= ~STA_NOINIT;
    } else {
        SD_SDIO_Stat |= STA_NOINIT;
    }

    // Ignoring Write protect
    SD_SDIO_Stat &= ~STA_PROTECT;

    return SD_SDIO_Stat;
}

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(
    BYTE pdrv /* Physical drive nmuber (0..) */
) {
    if (SD_Detect() != SD_PRESENT) {
        return STA_NOINIT;
    }

    // Ignoring Write protect
    SD_SDIO_Stat &= ~STA_PROTECT;

    return SD_SDIO_Stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(
    BYTE pdrv, /* Physical drive nmuber (0..) */
    BYTE* buff, /* Data buffer to store read data */
    DWORD sector, /* Sector address (LBA) */
    UINT count /* Number of sectors to read (1..128) */
) {
    /* Check count */
    if (!count) {
        return RES_PARERR;
    }
    PHAL_writeGPIO(SD_ACTIVITY_LED_PORT, SD_ACTIVITY_LED_PIN, 1);
    DRESULT res = _sdio_disk_read(buff, sector, count);
    PHAL_writeGPIO(SD_ACTIVITY_LED_PORT, SD_ACTIVITY_LED_PIN, 0);
    if (res != RES_OK) {
        // TODO: Log disk read error
        // DISKIO_LOG_RED("Disk read failed with res %d.\n", res);
    }
    return res;
}

static DRESULT _sdio_disk_read(
    BYTE* buff, /* Data buffer to store read data */
    DWORD sector, /* Sector address (LBA) */
    UINT count /* Number of sectors to read (1..128) */
) {
    SD_Error Status = SD_OK;

    if ((SD_SDIO_Stat & STA_NOINIT)) {
        return RES_NOTRDY;
    }

    if ((DWORD)buff & 3) {
        DRESULT res = RES_OK;
        DWORD scratch[BLOCK_SIZE / 4];

        while (count--) {
            res = _sdio_disk_read((void*)scratch, sector++, 1);

            if (res != RES_OK) {
                break;
            }

            memcpy(buff, scratch, BLOCK_SIZE);

            buff += BLOCK_SIZE;
        }

        return res;
    }

    Status = SD_ReadMultiBlocks(buff, sector << 9, BLOCK_SIZE, count);

    if (Status == SD_OK) {
        SDTransferState State;

        Status = SD_WaitReadOperation();

        while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY)
            ;

        if ((State == SD_TRANSFER_ERROR) || (Status != SD_OK)) {
            // TODO: log disk read transfer error
            // DISKIO_LOG_RED("Read transfer error, state %d, status %d\n", State, Status);
            return RES_ERROR;
        } else {
            return RES_OK;
        }
    } else {
        return RES_ERROR;
    }
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if _USE_WRITE
DRESULT disk_write(
    BYTE pdrv, /* Physical drive nmuber (0..) */
    const BYTE* buff, /* Data to be written */
    DWORD sector, /* Sector address (LBA) */
    UINT count /* Number of sectors to write (1..128) */
) {
    /* Check count */
    if (!count) {
        return RES_PARERR;
    }

    DRESULT res = _sdio_disk_write(buff, sector, count);

    if (res != RES_OK) {
        // TODO: Log disk write error
        // DISKIO_LOG_RED("Disk write failed with res %d.\n", res);
    }
    return res;
}

static DRESULT _sdio_disk_write(
    const BYTE* buff, /* Data to be written */
    DWORD sector, /* Sector address (LBA) */
    UINT count /* Number of sectors to write (1..128) */
) {
    SD_Error Status = SD_OK;
    uint32_t ulNotificationValue;

    if (SD_Detect() != SD_PRESENT) {
        return RES_NOTRDY;
    }

    // Shouldn't ever have to use scratch buffer because with new struct size buffer should always be at least 4 byte aligned?
    if ((DWORD)buff & 3) {
        DRESULT res = RES_OK;
        DWORD scratch[BLOCK_SIZE / 4];

        while (count--) {
            memcpy(scratch, buff, BLOCK_SIZE);
            res = _sdio_disk_write((void*)scratch, sector++, 1);

            if (res != RES_OK) {
                break;
            }

            buff += BLOCK_SIZE;
        }

        return (res);
    }

    (void)ulTaskNotifyTake(pdTRUE, 0); // clear any previous notifications
    sd_task_handle = xTaskGetCurrentTaskHandle();
    Status = SD_WriteMultiBlocks((uint8_t*)buff, sector << 9, BLOCK_SIZE, count); // 4GB Compliant
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, SD_BLOCKING_TIMEOUT_TICKS);
    sd_task_handle = nullptr;

    if (Status == SD_OK && ulNotificationValue) {
        SDTransferState State;

        Status = SD_WaitWriteOperation(); // Wait for SDIO transmission to finish before CMD12

        // Send CMD 13, requests SD card status (required after write/read operations) 
        while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY)
            ; // BUSY, OK (DONE), ERROR (FAIL)

        if ((State == SD_TRANSFER_ERROR) || (Status != SD_OK)) {
            return RES_ERROR;
        } else {
            return RES_OK;
        }
    } else {
        return RES_ERROR;
    }
}

#endif // _USE_WRITE

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
#if _USE_IOCTL
DRESULT disk_ioctl(
    BYTE pdrv, /* Physical drive nmuber (0..) */
    BYTE cmd, /* Control code */
    void* buff /* Buffer to send/receive control data */
) {
    switch (cmd) {
        case GET_SECTOR_SIZE: // Get R/W sector size (WORD)
            *(WORD*)buff = 512;
            break;
        case GET_BLOCK_SIZE: // Get erase block size in unit of sector (DWORD)
            *(DWORD*)buff = 32;
            break;
        case CTRL_SYNC:
        case CTRL_ERASE_SECTOR:
            break;
    }

    return RES_OK;
}
#endif // _USE_IOCTL

/*-----------------------------------------------------------------------*/
/* Get time for fatfs for files                                          */
/*-----------------------------------------------------------------------*/
DWORD get_fattime(void) {
    /* Returns current time packed into a DWORD variable */
    RTC_timestamp_t time;
    DWORD t;
    if (PHAL_getTimeRTC(&time)) {
        t = (time.date.year_bcd + 20) << 25;
        t |= (time.date.month_bcd) << 21;
        t |= (time.date.day_bcd) << 16;
        t |= (time.time.hours_bcd) << 11;
        t |= (time.time.minutes_bcd) << 5;
        t |= (time.time.seconds_bcd) >> 1;
        return t;
    } else {
        /* hard-coded date */
        return ((DWORD)(2013 - 1980) << 25) /* Year 2013 */
            | ((DWORD)7 << 21) /* Month 7 */
            | ((DWORD)28 << 16) /* Mday 28 */
            | ((DWORD)0 << 11) /* Hour 0 */
            | ((DWORD)0 << 5) /* Min 0 */
            | ((DWORD)0 >> 1); /* Sec 0 */
    }
}
