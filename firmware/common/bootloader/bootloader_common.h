
#ifndef PER_BOOTLOADER_COMMON_H
#define PER_BOOTLOADER_COMMON_H

#include <stdint.h>


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
#define BOOTLOADER_METADATA_FORMAT_VERSION 2U
/** Metadata flag set only by the final installation write. */
#define BOOTLOADER_METADATA_FLAG_INSTALLED_BY_BOOTLOADER (1U << 0U)
/**
 * @brief STM32G474RE flash layout shared with the linker scripts.
 *
 * Applications occupy the full 480 KiB remaining after the resident image and
 * metadata at BL_APP_ADDRESS. The metadata region and application slot begin
 * on flash erase-page boundaries;
 * PHAL_FLASH_erase() erases complete pages even when passed the metadata
 * record. Programming addresses are 8-byte aligned.
 *
 * Images and CRC input are 32-bit-word aligned. The package builder pads a
 * partial final word with erased bytes before recording size_bytes and crc32.
 * An odd number of image words is then padded with one erased 32-bit word for
 * the final 8-byte flash write; that physical padding is outside size_bytes.
 * The metadata record is written last and acts as the commit marker.
 */
#if defined(STM32G474xx)
#define BL_FLASH_BASE         0x08000000U
#define BL_FLASH_END          0x0807FFFFU
#define BL_METADATA_ADDRESS   0x08004000U
#define BL_METADATA_REGION_SIZE (16U * 1024U)
#define BL_APP_ADDRESS        0x08008000U
#define BL_APP_SLOT_SIZE      (480U * 1024U)
#define BL_APP_END            (BL_APP_ADDRESS + BL_APP_SLOT_SIZE - 1U)
#define BL_RESERVED_ADDRESS   (BL_FLASH_END + 1U)
#define BL_RESERVED_SIZE      0U
/** Start of the resident bootloader region applications may reprogram. */
#define BL_BOOTLOADER_ADDRESS BL_FLASH_BASE
/** Size of the resident bootloader region applications may reprogram. */
#define BL_BOOTLOADER_REGION_SIZE (BL_METADATA_ADDRESS - BL_FLASH_BASE)

_Static_assert(BL_METADATA_ADDRESS == (BL_FLASH_BASE + (16U * 1024U)),
               "Bootloader metadata must follow the 16 KiB resident image");
_Static_assert(BL_APP_ADDRESS == (BL_METADATA_ADDRESS + BL_METADATA_REGION_SIZE),
               "Application must follow the 16 KiB metadata region");
_Static_assert(BL_APP_END == BL_FLASH_END && BL_APP_END == 0x0807FFFFU,
               "Application slot must consume the remaining STM32G474 flash");
_Static_assert(BL_RESERVED_ADDRESS == 0x08080000U && BL_RESERVED_SIZE == 0U,
               "There must be no flash reserved after the application slot");
#endif

/** Size of the committed metadata record. */
#define BL_METADATA_SIZE 32U
/** Wire and CRC payload granularity. */
#define BL_WORD_SIZE 4U
/** STM32G4 flash programming granularity. */
#define BL_FLASH_WRITE_SIZE 8U

/**
 * @brief Metadata describing the only application the bootloader may launch.
 *
 * This record is written after the image at BL_APP_ADDRESS has passed sequence,
 * CRC, and vector validation. A missing or inconsistent record prevents
 * application launch.
 */
typedef struct __attribute__((aligned(BL_FLASH_WRITE_SIZE))) {
    uint32_t magic;          /**< Must equal BOOTLOADER_METADATA_MAGIC. */
    uint32_t format_version; /**< Must equal BOOTLOADER_METADATA_FORMAT_VERSION. */
    uint32_t flags;          /**< Must include INSTALLED_BY_BOOTLOADER. */
    uint32_t crc32;          /**< CRC-32/MPEG-2 over the application words. */
    uint32_t address;        /**< Must equal BL_APP_ADDRESS. */
    uint32_t size_bytes;     /**< Non-zero, word-aligned application length. */
    uint32_t bootloader_git_hash; /**< Resident bootloader build advertised by the app. */
    uint32_t reserved;       /**< Reserved for a future metadata format. */
} BootloaderMetadata_t;

_Static_assert(sizeof(BootloaderMetadata_t) == BL_METADATA_SIZE,
               "Bootloader metadata must occupy one 32-byte record");
_Static_assert((sizeof(BootloaderMetadata_t) % BL_FLASH_WRITE_SIZE) == 0U,
               "Bootloader metadata must be flash-write aligned");

/** Return the installed resident bootloader hash, or zero when unavailable. */
uint32_t BL_getGitHash(void);

/**
 * @brief Begin reprogramming the resident bootloader from the application.
 *
 * Erases the complete bootloader region and prepares to accept the image words.
 * A bootloader-aware application handles this protocol while it is running; the
 * resident bootloader cannot reprogram the region it executes from. Replies
 * with BOOTLOADER_STATUS_ACK(size_bytes) or BOOTLOADER_STATUS_ERROR.
 *
 * @param size_bytes Word-aligned image size no larger than BL_BOOTLOADER_REGION_SIZE.
 * @return true when the region is erased and index zero is accepted.
 */
bool BL_bootloaderUpdateStart(uint32_t size_bytes);

/**
 * @brief Program one sequential image word into the bootloader region.
 *
 * Duplicate indices are ignored; a gap cancels the transfer. Pairs of words are
 * written as one 64-bit flash word, padding an unpaired final word with 0xFF.
 * Replies with BOOTLOADER_STATUS_ERROR on a locked, sequence, address, or flash
 * failure.
 *
 * @param index Zero-based word index within the announced image.
 * @param word  Image word at @p index.
 * @return true when the word is accepted or already programmed.
 */
bool BL_bootloaderUpdateWord(uint32_t index, uint32_t word);

/**
 * @brief Validate the received bootloader image and confirm it.
 *
 * Verifies the image CRC, initial stack pointer, and reset vector before
 * replying BOOTLOADER_STATUS_ACK(calculated_crc) or BOOTLOADER_STATUS_CRC_ERROR.
 *
 * @param expected_crc Host-computed CRC-32/MPEG-2 over the image words.
 * @return true when the image passes validation and is confirmed.
 */
bool BL_bootloaderUpdateCrc(uint32_t expected_crc);

/**
 * @brief Reset the device so the newly programmed resident bootloader runs.
 *
 * @return Never; the device restarts.
 */
bool BL_bootloaderUpdateJump(void);

/** @brief Detail values accompanying BOOTLOADER_STATUS_ERROR. */
typedef enum {
    BLERROR_NONE     = 0x00,
    BLERROR_LOCKED   = 0x01, /**< Data arrived before START. */
    BLERROR_SEQUENCE = 0x02, /**< A word was skipped or commit was premature. */
    BLERROR_FLASH    = 0x03, /**< Flash erase, programming, or verify failed. */
    BLERROR_SIZE     = 0x04, /**< Image size is empty, unaligned, or too large. */
    BLERROR_ADDRESS  = 0x05, /**< Image, vector, or metadata address is invalid. */
} BLError_t;


#endif
