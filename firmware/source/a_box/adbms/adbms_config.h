/**
 * @file adbms_config.h
 * @brief Shared pack topology configuration for the ADBMS driver.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef ADBMS_CONFIG_H
#define ADBMS_CONFIG_H

#include <stddef.h>

// Number of physical battery modules in the pack
static constexpr size_t ADBMS_MODULE_COUNT = 5U;

// Number of ADBMS chips per physical battery module
static constexpr size_t ADBMS_CHIPS_PER_MODULE = 2U;

// Total number of ADBMS chips in the daisy chain
static constexpr size_t ADBMS_CHIP_COUNT = ADBMS_MODULE_COUNT * ADBMS_CHIPS_PER_MODULE;

// Number of series cells per physical battery module (16 on first chip, 13 on second chip)
static constexpr size_t ADBMS_CELLS_PER_MODULE = 29U;

// First 9 of the 10 GPIOs on a chip are cell thermistors
// Last is an on board thermistor
static constexpr size_t ADBMS_CELL_THERMS_PER_CHIP  = 9U;
static constexpr size_t ADBMS_BOARD_THERMS_PER_CHIP = 1U;

// Number of cell thermistors per physical battery module
static constexpr size_t ADBMS_CELL_THERMS_PER_MODULE = ADBMS_CHIPS_PER_MODULE * ADBMS_CELL_THERMS_PER_CHIP;

// Number of onboard PCB thermistors per physical battery module
static constexpr size_t ADBMS_BOARD_THERMS_PER_MODULE = ADBMS_CHIPS_PER_MODULE * ADBMS_BOARD_THERMS_PER_CHIP;

// Total number of temperature sensors per physical battery module
static constexpr size_t ADBMS_THERMISTORS_PER_MODULE = ADBMS_CELL_THERMS_PER_MODULE + ADBMS_BOARD_THERMS_PER_MODULE;

#endif // ADBMS_CONFIG_H
