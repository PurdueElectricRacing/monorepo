/**
 * @file flash.c
 * @brief STM32G4 internal-flash public-layer implementation.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * This layer validates byte ranges, derives page/programming boundaries, and
 * verifies results without exposing FLASH controller registers to callers.
 */

#include "common/phal_G4/flash/flash.h"

#include "common/phal_G4/flash/flash_priv.h"
#include "stm32g474xx.h"

/** Validate an absolute flash address range without wrapping arithmetic. */
static bool flash_range_is_valid(uint32_t address, uint32_t length_bytes) {
    if (length_bytes == 0U) {
        return true;
    }

    if (address < FLASH_BASE) {
        return false;
    }

    uint32_t flash_size_bytes = FLASH_PRIV_get_size_bytes();
    uint32_t flash_offset     = address - FLASH_BASE;
    return flash_offset < flash_size_bytes && length_bytes <= flash_size_bytes - flash_offset;
}

/**
 * Validate write alignment and round the requested range to double-word size.
 * The rounded range is checked because padding bytes are also programmed.
 */
static bool flash_write_range_is_valid(uint32_t address,
                                   uint32_t length_bytes,
                                   uint32_t *program_length_bytes) {
    if ((address % PHAL_FLASH_WRITE_ALIGNMENT_BYTES) != 0U) {
        return false;
    }

    if (length_bytes > UINT32_MAX - (PHAL_FLASH_WRITE_ALIGNMENT_BYTES - 1U)) {
        return false;
    }

    *program_length_bytes =
        (length_bytes + PHAL_FLASH_WRITE_ALIGNMENT_BYTES - 1U)
        & ~(PHAL_FLASH_WRITE_ALIGNMENT_BYTES - 1U);
    return flash_range_is_valid(address, *program_length_bytes);
}

/** Confirm every double word in a write range is still erased (all 0xFF). */
static bool flash_range_is_erased(uint32_t address, uint32_t length_bytes) {
    for (uint32_t offset = 0U; offset < length_bytes;
         offset += PHAL_FLASH_WRITE_ALIGNMENT_BYTES) {
        volatile const uint32_t *target = (volatile const uint32_t *)(address + offset);
        if (target[0] != UINT32_MAX || target[1] != UINT32_MAX) {
            return false;
        }
    }

    return true;
}

/** Build an erased-padded 64-bit value for one programming operation. */
static uint64_t flash_build_double_word(const uint8_t *source, uint32_t length_bytes) {
    uint64_t data       = UINT64_MAX;
    uint32_t copy_bytes = length_bytes;
    if (copy_bytes > PHAL_FLASH_WRITE_ALIGNMENT_BYTES) {
        copy_bytes = PHAL_FLASH_WRITE_ALIGNMENT_BYTES;
    }

    for (uint32_t i = 0U; i < copy_bytes; i++) {
        uint64_t byte_mask = (uint64_t)UINT8_MAX << (i * 8U);
        data = (data & ~byte_mask) | ((uint64_t)source[i] << (i * 8U));
    }

    return data;
}

/** Verify both 32-bit halves after a double-word program operation. */
static bool flash_double_word_matches(uint32_t address, uint64_t expected) {
    volatile const uint32_t *actual = (volatile const uint32_t *)address;
    return actual[0] == (uint32_t)expected && actual[1] == (uint32_t)(expected >> 32U);
}

/** Verify every word in a complete erased page. */
static bool flash_page_is_erased(uint32_t page_address, uint32_t page_size_bytes) {
    volatile const uint32_t *page = (volatile const uint32_t *)page_address;
    uint32_t word_count           = page_size_bytes / sizeof(uint32_t);

    for (uint32_t i = 0U; i < word_count; i++) {
        if (page[i] != UINT32_MAX) {
            return false;
        }
    }

    return true;
}

bool PHAL_FLASH_read(uint32_t address, void *destination, uint32_t length_bytes) {
    // Zero-length operations are successful no-ops, including with null pointers.
    if (length_bytes == 0U) {
        return true;
    }

    if (destination == nullptr || !flash_range_is_valid(address, length_bytes)) {
        return false;
    }

    if (!FLASH_PRIV_begin_read()) {
        return false;
    }

    // Read through a volatile pointer so the compiler performs flash accesses.
    volatile const uint8_t *source = (volatile const uint8_t *)address;
    uint8_t *output                = destination;
    for (uint32_t i = 0U; i < length_bytes; i++) {
        output[i] = source[i];
    }

    return FLASH_PRIV_end_read();
}

bool PHAL_FLASH_write(uint32_t address, const void *source, uint32_t length_bytes) {
    // Validate the complete padded range before unlocking or modifying FLASH.
    if (length_bytes == 0U) {
        return true;
    }

    uint32_t program_length_bytes = 0U;
    if (source == nullptr
        || !flash_write_range_is_valid(address, length_bytes, &program_length_bytes)) {
        return false;
    }

    flash_operation_context_t context = {0};
    if (!FLASH_PRIV_begin_operation(&context)) {
        return false;
    }

    // Refuse the entire write if any target bit is not in erased state.
    bool success = flash_range_is_erased(address, program_length_bytes);
    const uint8_t *input = source;

    for (uint32_t offset = 0U; success && offset < program_length_bytes;
         offset += PHAL_FLASH_WRITE_ALIGNMENT_BYTES) {
        // The final padded double word uses 0xFF for bytes beyond the request.
        uint32_t remaining_bytes = length_bytes - offset;
        uint64_t data            = flash_build_double_word(input + offset, remaining_bytes);
        if (data == UINT64_MAX) {
            continue;
        }

        uint32_t target_address = address + offset;
        success = FLASH_PRIV_program_double_word(target_address, data)
            && flash_double_word_matches(target_address, data);
    }

    // Always restore caches and relock the controller before returning.
    bool cleanup_success = FLASH_PRIV_end_operation(&context);
    return success && cleanup_success;
}

bool PHAL_FLASH_erase(uint32_t address, uint32_t length_bytes) {
    if (length_bytes == 0U) {
        return true;
    }

    if (!flash_range_is_valid(address, length_bytes)) {
        return false;
    }

    // Convert the byte range to inclusive first/last page indices. The public
    // contract intentionally erases complete pages touched by the range.
    uint32_t page_size_bytes = FLASH_PRIV_get_page_size_bytes();
    uint32_t flash_offset     = address - FLASH_BASE;
    uint32_t first_page       = flash_offset / page_size_bytes;
    uint32_t last_page        = (flash_offset + length_bytes - 1U) / page_size_bytes;

    flash_operation_context_t context = {0};
    if (!FLASH_PRIV_begin_operation(&context)) {
        return false;
    }

    bool success = true;
    for (uint32_t page = first_page; success && page <= last_page; page++) {
        // Verify each page immediately so a later failure cannot hide an earlier one.
        uint32_t page_address = FLASH_BASE + page * page_size_bytes;
        success = FLASH_PRIV_erase_page(page_address)
            && flash_page_is_erased(page_address, page_size_bytes);
    }

    bool cleanup_success = FLASH_PRIV_end_operation(&context);
    return success && cleanup_success;
}
