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

// Number of thermistors per physical battery module (10 per chip)
static constexpr size_t ADBMS_THERMISTORS_PER_MODULE = 20U;

#endif // ADBMS_CONFIG_H
