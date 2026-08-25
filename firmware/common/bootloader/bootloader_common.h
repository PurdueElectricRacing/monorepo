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

#include "can_library/generated/can_types.h"

/** Bootloader wire-protocol version reported with BOOTLOADER_STATUS_READY. */
#define BOOTLOADER_PROTOCOL_VERSION 1U
/** Period between resident bootloader information frames. */
#define BOOTLOADER_INFO_PERIOD_MS 5000U
/** Information-frame flag indicating that the bootloader accepts updates. */
#define BOOTLOADER_INFO_FLAG_BOOTLOADABLE (1U << 0U)
/** Information-frame flag indicating that the resident image is ready. */
#define BOOTLOADER_INFO_FLAG_READY (1U << 1U)
/** Magic identifying a BootloaderMetadata_t record (ASCII "PERB"). */
#define BOOTLOADER_METADATA_MAGIC 0x50455242U
/** Metadata layout version accepted by resident bootloaders. */
#define BOOTLOADER_METADATA_FORMAT_VERSION 1U
/** Metadata flag set only by the final installation write. */
#define BOOTLOADER_METADATA_FLAG_INSTALLED_BY_BOOTLOADER (1U << 0U)
/** Magic identifying a valid .noinit reset request. */
#define BOOTLOADER_SHARED_MEMORY_MAGIC 0xABCDBEEFU

/**
 * @brief STM32G474RE flash layout shared with the linker scripts.
 *
 * Applications occupy a fixed 160 KiB slot at BL_APP_ADDRESS. Incoming images
 * are written to the equally sized staging slot and copied only after CRC
 * validation. All three regions begin on 2 KiB erase-page boundaries and do
 * not overlap; PHAL_FLASH_erase() erases complete pages even when passed the
 * metadata record. Programming addresses are 8-byte aligned.
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
#define BL_METADATA_SIZE 24U
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
typedef struct __attribute__((aligned(BL_FLASH_WRITE_SIZE))) {
    uint32_t magic;          /**< Must equal BOOTLOADER_METADATA_MAGIC. */
    uint32_t format_version; /**< Must equal BOOTLOADER_METADATA_FORMAT_VERSION. */
    uint32_t flags;          /**< Must include INSTALLED_BY_BOOTLOADER. */
    uint32_t crc32;          /**< CRC-32/MPEG-2 over the application words. */
    uint32_t address;        /**< Must equal BL_APP_ADDRESS. */
    uint32_t size_bytes;     /**< Non-zero, word-aligned application length. */
} BootloaderMetadata_t;

_Static_assert(sizeof(BootloaderMetadata_t) == BL_METADATA_SIZE,
               "Bootloader metadata must occupy one 24-byte record");
_Static_assert((sizeof(BootloaderMetadata_t) % BL_FLASH_WRITE_SIZE) == 0U,
               "Bootloader metadata must be flash-write aligned");

/** @brief Detail values accompanying BOOTLOADER_STATUS_ERROR. */
typedef enum {
    BLERROR_NONE = 0x00,
    BLERROR_LOCKED = 0x01,   /**< Data arrived before START. */
    BLERROR_SEQUENCE = 0x02, /**< A word was skipped or commit was premature. */
    BLERROR_FLASH = 0x03,    /**< Flash erase, programming, or verify failed. */
    BLERROR_SIZE = 0x04,     /**< Image size is empty, unaligned, or too large. */
    BLERROR_ADDRESS = 0x05,  /**< Image, vector, or metadata address is invalid. */
} BLError_t;

/** @brief Reasons retained across an application-requested system reset. */
typedef enum {
    RESET_REASON_INVALID = 0,      /**< No valid application reset request. */
    RESET_REASON_DOWNLOAD_FW,      /**< Stay resident and accept an update. */
} ResetReason_t;

/** Stable target identifiers carried by bootloader information frames. */
typedef enum {
    BOOTLOADER_TARGET_MAIN_MODULE = 1,
    BOOTLOADER_TARGET_DASHBOARD,
    BOOTLOADER_TARGET_A_BOX,
    BOOTLOADER_TARGET_TORQUE_VECTOR,
    BOOTLOADER_TARGET_FRONT_DRIVELINE,
    BOOTLOADER_TARGET_REAR_DRIVELINE,
} BootloaderTarget_t;

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
