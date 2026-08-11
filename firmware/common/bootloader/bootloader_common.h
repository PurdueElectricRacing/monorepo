/**
 * @file bootloader_common.h
 * @brief Shared G4 CAN bootloader protocol and application hand-off API.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * Resident bootloaders and bootloader-aware applications include this header so
 * wire values, flash addresses, metadata, and reset markers remain identical.
 * CRC protects against corruption; it does not authenticate firmware.
 */

#ifndef PER_BOOTLOADER_COMMON_H
#define PER_BOOTLOADER_COMMON_H

#include <stdbool.h>
#include <stdint.h>

/** Bootloader wire-protocol version reported with BLSTAT_READY. */
#define BOOTLOADER_PROTOCOL_VERSION 1U
/** Magic identifying a committed BootloaderMetadata_t record (ASCII "PERB"). */
#define BOOTLOADER_METADATA_MAGIC 0x50455242U
/** Magic identifying a valid .noinit reset request. */
#define BOOTLOADER_SHARED_MEMORY_MAGIC 0xABCDBEEFU

/**
 * @brief STM32G474RE flash layout shared with the linker scripts.
 *
 * Applications occupy a fixed 160 KiB slot at BL_APP_ADDRESS. Incoming images
 * are written to the equally sized staging slot and copied only after CRC
 * validation. All three regions begin on 2 KiB erase-page boundaries and do
 * not overlap; PHAL_FLASH_erase() erases complete pages even when passed the
 * 16-byte metadata record. Programming addresses are 8-byte aligned.
 *
 * Images and CRC input are 32-bit-word aligned. An odd number of image words is
 * padded with one erased 32-bit word for the final 8-byte flash write, but that
 * physical padding is outside size_bytes and is not included in the image CRC.
 * The metadata page is written last and acts as the commit marker.
 */
#if defined(STM32G474xx)
#define BL_FLASH_BASE 0x08000000U
#define BL_FLASH_END 0x0807FFFFU
#define BL_METADATA_ADDRESS 0x08004000U
#define BL_APP_ADDRESS 0x08008000U
#define BL_STAGING_ADDRESS 0x08030000U
#define BL_APP_SLOT_SIZE (160U * 1024U)
#endif

/** Size of the committed metadata record. */
#define BL_METADATA_SIZE 16U
/** Wire and CRC payload granularity. */
#define BL_WORD_SIZE 4U
/** STM32G4 flash programming granularity. */
#define BL_FLASH_WRITE_SIZE 8U

/**
 * @brief Metadata describing the only application the bootloader may launch.
 *
 * This record is written after the staged image has passed CRC validation and
 * has been copied to BL_APP_ADDRESS. A missing or inconsistent record prevents
 * application launch.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;      /**< Must equal BOOTLOADER_METADATA_MAGIC. */
    uint32_t crc32;      /**< CRC-32/MPEG-2 over the application words. */
    uint32_t address;    /**< Must equal BL_APP_ADDRESS. */
    uint32_t size_bytes; /**< Non-zero, word-aligned application length. */
} BootloaderMetadata_t;

_Static_assert(sizeof(BootloaderMetadata_t) == BL_METADATA_SIZE,
               "Bootloader metadata must occupy one 16-byte record");

/** @brief Commands carried in byte 0 of a five-byte command frame. */
typedef enum {
    BLCMD_START = 0x01, /**< Erase staging and begin an ordered word stream. */
    BLCMD_CRC = 0x03,   /**< Validate staging, install it, and commit metadata. */
    BLCMD_JUMP = 0x04,  /**< Validate and launch the committed application. */
} BLCmd_t;

/**
 * @brief Status values carried in byte 0 of a five-byte response frame.
 *
 * Response detail occupies bytes 1 through 4 as a little-endian uint32_t.
 */
typedef enum {
    BLSTAT_READY = 0x00,     /**< Listening; detail is the protocol version. */
    BLSTAT_ACK = 0x01,       /**< Accepted; detail depends on the command. */
    BLSTAT_ERROR = 0x02,     /**< Rejected; detail is a BLError_t. */
    BLSTAT_CRC_ERROR = 0x03, /**< CRC mismatch; detail is the calculated CRC. */
    BLSTAT_UNKNOWN_CMD = 0x04, /**< Command byte is not recognized. */
} BLStatus_t;

/** @brief Detail values accompanying BLSTAT_ERROR. */
typedef enum {
    BLERROR_NONE = 0x00,
    BLERROR_LOCKED = 0x01,   /**< Data arrived before BLCMD_START. */
    BLERROR_SEQUENCE = 0x02, /**< A word was skipped or commit was premature. */
    BLERROR_FLASH = 0x03,    /**< Flash erase, programming, or verify failed. */
    BLERROR_SIZE = 0x04,     /**< Image size is empty, unaligned, or too large. */
    BLERROR_ADDRESS = 0x05,  /**< Image, vector, or metadata address is invalid. */
} BLError_t;

/** @brief Reasons retained across an application-requested system reset. */
typedef enum {
    RESET_REASON_INVALID = 0,      /**< No valid application reset request. */
    RESET_REASON_DOWNLOAD_FW,     /**< Stay resident and accept an update. */
} ResetReason_t;

/**
 * @brief One-shot application-to-bootloader request stored in .noinit RAM.
 *
 * The bootloader requires both fields to match and clears them before deciding
 * whether to launch the installed application.
 */
typedef struct {
    uint32_t magic_word;        /**< BOOTLOADER_SHARED_MEMORY_MAGIC when valid. */
    ResetReason_t reset_reason; /**< Requested action after reset. */
} BootloaderSharedMemory_t;

/** Shared .noinit request record defined by bootloader_common.c. */
extern BootloaderSharedMemory_t bootloader_shared_memory;

/**
 * @brief Reset into the resident CAN bootloader.
 *
 * Writes the one-shot download marker, then calls NVIC_SystemReset(). The
 * resident image consumes and clears the marker during startup.
 */
void Bootloader_ResetForFirmwareDownload(void);

#endif /* PER_BOOTLOADER_COMMON_H */
