/**
 * @file adbms_config.h
 * @brief Shared pack topology and physical-to-chain mapping configuration for the ADBMS driver.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef ADBMS_CONFIG_H
#define ADBMS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

// Number of physical battery modules in the pack.
static constexpr size_t ADBMS_MODULE_COUNT = 5U;

// Number of ADBMS chips per physical battery module (Chip A and Chip B).
static constexpr size_t ADBMS_CHIPS_PER_MODULE = 2U;

// Total number of ADBMS chips in the isoSPI daisy chain.
static constexpr size_t ADBMS_CHIP_COUNT = ADBMS_MODULE_COUNT * ADBMS_CHIPS_PER_MODULE;

// Connected series cells on Chip A of each physical module (channels 1..16, zero-based 0..15).
static constexpr size_t ADBMS_CHIP_A_CELL_COUNT = 16U;

// Connected series cells on Chip B of each physical module (channels 1..13, zero-based 0..12;
// channels 14..16 are shorted/absent).
static constexpr size_t ADBMS_CHIP_B_CELL_COUNT = 13U;

// Connected series cells per physical battery module.
static constexpr size_t ADBMS_CELLS_PER_MODULE = ADBMS_CHIP_A_CELL_COUNT + ADBMS_CHIP_B_CELL_COUNT;

// Total connected series cells in the battery pack.
static constexpr size_t ADBMS_PACK_CELL_COUNT = ADBMS_MODULE_COUNT * ADBMS_CELLS_PER_MODULE;

// Populated thermistor channels per ADBMS chip (GPIO 1..10, zero-based 0..9).
static constexpr size_t ADBMS_THERMISTORS_PER_CHIP = 10U;

// Populated thermistor channels per physical battery module.
static constexpr size_t ADBMS_THERMISTORS_PER_MODULE =
    ADBMS_CHIPS_PER_MODULE * ADBMS_THERMISTORS_PER_CHIP;

// Total populated thermistor channels in the battery pack.
static constexpr size_t ADBMS_PACK_THERMISTOR_COUNT =
    ADBMS_MODULE_COUNT * ADBMS_THERMISTORS_PER_MODULE;

static_assert(ADBMS_CHIPS_PER_MODULE == 2U);
static_assert(ADBMS_CHIP_COUNT == 10U);
static_assert(ADBMS_CELLS_PER_MODULE == 29U);
static_assert(ADBMS_PACK_CELL_COUNT == 145U);
static_assert(ADBMS_THERMISTORS_PER_MODULE == 20U);
static_assert(ADBMS_PACK_THERMISTOR_COUNT == 100U);

/**
 * @brief Role of an ADBMS chip within a physical battery module.
 *
 * Chip A measures module cells `0..15` (1-based `1..16`, raw channels `0..15`) and
 * module thermistors `0..9` (1-based `1..10`, raw GPIOs `0..9`).
 * Chip B measures module cells `16..28` (1-based `17..29`, raw channels `0..12`;
 * raw channels `13..15` are unused/shorted) and module thermistors `10..19`
 * (1-based `11..20`, raw GPIOs `0..9`).
 */
typedef enum : uint8_t {
    ADBMS_CHIP_ROLE_A = 0U,
    ADBMS_CHIP_ROLE_B = 1U,
} adbms_chip_role_t;

/**
 * @brief Physical module and chip role (`ADBMS_CHIP_ROLE_A` / `ADBMS_CHIP_ROLE_B`) to raw
 * daisy-chain receive-order chip index (`0 .. ADBMS_CHIP_COUNT - 1`).
 *
 * @note Assumed sequential receive order (`{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}`)
 * pending full schematic and hardware isoSPI daisy-chain verification.
 */
static constexpr size_t ADBMS_MODULE_CHIP_INDICES[ADBMS_MODULE_COUNT][ADBMS_CHIPS_PER_MODULE] = {
    [0] = {[ADBMS_CHIP_ROLE_A] = 0U, [ADBMS_CHIP_ROLE_B] = 1U},
    [1] = {[ADBMS_CHIP_ROLE_A] = 2U, [ADBMS_CHIP_ROLE_B] = 3U},
    [2] = {[ADBMS_CHIP_ROLE_A] = 4U, [ADBMS_CHIP_ROLE_B] = 5U},
    [3] = {[ADBMS_CHIP_ROLE_A] = 6U, [ADBMS_CHIP_ROLE_B] = 7U},
    [4] = {[ADBMS_CHIP_ROLE_A] = 8U, [ADBMS_CHIP_ROLE_B] = 9U},
};

/**
 * @brief Raw daisy-chain chip index and chip-local channel index.
 */
typedef struct {
    /** Raw receive-order chip index (`0 .. ADBMS_CHIP_COUNT - 1`). */
    size_t chip_idx;
    /** Chip-local channel index (`0 .. 15` for cells, `0 .. 9` for GPIOs/thermistors). */
    size_t channel_idx;
} adbms_channel_map_t;

/**
 * @brief Resolve the raw receive-order chip index for a physical module and chip role.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `role < ADBMS_CHIPS_PER_MODULE`
 */
static inline size_t ADBMS_module_chip_index(size_t module_idx, adbms_chip_role_t role) {
    return ADBMS_MODULE_CHIP_INDICES[module_idx][role];
}

/**
 * @brief Map a physical module and module-local cell index to a raw chip and cell channel.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `cell_idx < ADBMS_CELLS_PER_MODULE` (`0 .. 28`)
 */
static inline adbms_channel_map_t ADBMS_map_cell(size_t module_idx, size_t cell_idx) {
    if (cell_idx < ADBMS_CHIP_A_CELL_COUNT) {
        return (adbms_channel_map_t){
            .chip_idx    = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_A),
            .channel_idx = cell_idx,
        };
    }
    return (adbms_channel_map_t){
        .chip_idx    = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_B),
        .channel_idx = cell_idx - ADBMS_CHIP_A_CELL_COUNT,
    };
}

/**
 * @brief Map a physical module and module-local thermistor index to a raw chip and GPIO channel.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `therm_idx < ADBMS_THERMISTORS_PER_MODULE` (`0 .. 19`)
 */
static inline adbms_channel_map_t ADBMS_map_therm(size_t module_idx, size_t therm_idx) {
    if (therm_idx < ADBMS_THERMISTORS_PER_CHIP) {
        return (adbms_channel_map_t){
            .chip_idx    = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_A),
            .channel_idx = therm_idx,
        };
    }
    return (adbms_channel_map_t){
        .chip_idx    = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_B),
        .channel_idx = therm_idx - ADBMS_THERMISTORS_PER_CHIP,
    };
}

#endif // ADBMS_CONFIG_H
