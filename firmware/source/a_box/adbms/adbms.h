/**
 * @file adbms.h
 * @brief Primary logic and interface for ADBMS battery management system driver.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef ADBMS_H
#define ADBMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adbms6380.h"
#include "adbms_config.h"
#include "common/phal_G4/spi/spi.h"

static_assert(ADBMS_CHIP_A_CELL_COUNT <= ADBMS6380_CELL_COUNT);
static_assert(ADBMS_CHIP_B_CELL_COUNT <= ADBMS6380_CELL_COUNT);
static_assert(ADBMS_THERMISTORS_PER_CHIP == ADBMS6380_GPIO_COUNT);

/**
 * @brief ADBMS driver connection/operation state.
 */
typedef enum : uint8_t {
    ADBMS_STATE_CONNECTING = 0, // Connect sequence (write then verify for rega/regb, start conversions)
    ADBMS_STATE_READ_LOOP  = 1, // Measure/balance cycle (cells, temps, balance, REGB write/verify)
    ADBMS_STATE_RECOVERING = 2, // Non-blocking wait for a full chip sleep-wake cycle
    ADBMS_STATE_FATAL      = 3, // Latching terminal state after repeated failed recovery
} adbms_state_t;

/**
 * @brief Top-level ADBMS driver state, device handle, and pack-level statistics.
 */
typedef struct {
    /** Low-level ADBMS6380 daisy-chain device state, SPI buffers, and error flags. */
    adbms6380_t dev;

    /** Minimum connected cell voltage across all physical modules (volts). */
    float min_voltage;
    /** Maximum connected cell voltage across all physical modules (volts). */
    float max_voltage;
    /** Average connected cell voltage across all physical modules (volts). */
    float avg_voltage;
    /** Sum of connected cell voltages across all physical modules (volts). */
    float sum_voltage;

    /** Minimum thermistor temperature across all physical modules (Celsius). */
    float min_therm_temp;
    /** Maximum thermistor temperature across all physical modules (Celsius). */
    float max_therm_temp;
    /** Average thermistor temperature across all physical modules (Celsius). */
    float avg_therm_temp;

    /** Time captured when entering RECOVERING. */
    uint32_t recover_start_tick_ms;
    /** Current driver state. */
    adbms_state_t state;
    /** Consecutive recovery cycles (sleep-wake retries) without a successful connect. */
    uint8_t recovery_attempts;
    /** true if cell discharge balancing is permitted by higher-level logic. */
    bool is_balancing_enabled;
} adbms_bms_t;

/**
 * @brief Initialize the ADBMS driver instance and underlying ADBMS6380 device chain.
 *
 * Sets the driver state to CONNECTING, clears discharge and error flags,
 * deasserts CS, and configures the internal SPI TX/RX buffers.
 *
 * @param bms Non-null pointer to driver state to initialize.
 * @param spi Non-null SPI configuration used for ADBMS transactions.
 */
void ADBMS_init(adbms_bms_t *bms, SPI_InitConfig_t *spi);

/**
 * @brief Periodic service routine for connection, measurements, and balancing.
 *
 * @param bms Non-null pointer to driver state.
 * @param min_voltage_for_balance Minimum pack voltage required to allow balancing.
 * @param min_delta_for_balance Voltage delta above minimum to start discharging.
 */
void ADBMS_periodic(adbms_bms_t *bms, float min_voltage_for_balance, float min_delta_for_balance);

/**
 * @brief Get the latest voltage reading (volts) for a physical module and cell.
 *
 * @param bms Non-null pointer to driver state.
 * @param module_idx Physical module index (`0 .. ADBMS_MODULE_COUNT - 1`).
 * @param cell_idx Physical cell index within the module (`0 .. ADBMS_CELLS_PER_MODULE - 1`).
 * @return Cell voltage in volts.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `cell_idx < ADBMS_CELLS_PER_MODULE` (`0 .. 28`)
 */
static inline float ADBMS_get_cell_voltage(
    const adbms_bms_t *bms,
    size_t module_idx,
    size_t cell_idx
) {
    adbms_channel_map_t map = ADBMS_map_cell(module_idx, cell_idx);
    return bms->dev.chips[map.chip_idx].cell_voltages[map.channel_idx];
}

/**
 * @brief Get the latest thermistor temperature (Celsius) for a physical module and channel.
 *
 * @param bms Non-null pointer to driver state.
 * @param module_idx Physical module index (`0 .. ADBMS_MODULE_COUNT - 1`).
 * @param therm_idx Logical thermistor index within the module (`0 .. ADBMS_THERMISTORS_PER_MODULE - 1`).
 * @return Thermistor temperature in Celsius.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `therm_idx < ADBMS_THERMISTORS_PER_MODULE` (`0 .. 19`)
 */
static inline float ADBMS_get_therm_temp(
    const adbms_bms_t *bms,
    size_t module_idx,
    size_t therm_idx
) {
    adbms_channel_map_t map = ADBMS_map_therm(module_idx, therm_idx);
    return bms->dev.chips[map.chip_idx].therms_temps[map.channel_idx];
}

/**
 * @brief Check whether a specific cell is currently discharging (balancing).
 *
 * @param bms Non-null pointer to driver state.
 * @param module_idx Physical module index (`0 .. ADBMS_MODULE_COUNT - 1`).
 * @param cell_idx Physical cell index within the module (`0 .. ADBMS_CELLS_PER_MODULE - 1`).
 * @return true if balancing discharge is requested for this cell.
 *
 * @pre `module_idx < ADBMS_MODULE_COUNT` (`0 .. 4`)
 * @pre `cell_idx < ADBMS_CELLS_PER_MODULE` (`0 .. 28`)
 */
static inline bool ADBMS_is_cell_discharging(
    const adbms_bms_t *bms,
    size_t module_idx,
    size_t cell_idx
) {
    adbms_channel_map_t map = ADBMS_map_cell(module_idx, cell_idx);
    return bms->dev.chips[map.chip_idx].is_discharging[map.channel_idx];
}

/// Get the minimum connected cell voltage across all modules in volts
static inline float ADBMS_get_min_voltage(const adbms_bms_t *bms) {
    return bms->min_voltage;
}

/// Get the maximum connected cell voltage across all modules in volts
static inline float ADBMS_get_max_voltage(const adbms_bms_t *bms) {
    return bms->max_voltage;
}

/// Get the sum of all connected cell voltages across all modules in volts
static inline float ADBMS_get_sum_voltage(const adbms_bms_t *bms) {
    return bms->sum_voltage;
}

/// Get the minimum thermistor temperature across all modules in Celsius
static inline float ADBMS_get_min_therm_temp(const adbms_bms_t *bms) {
    return bms->min_therm_temp;
}

/// Get the maximum thermistor temperature across all modules in Celsius
static inline float ADBMS_get_max_therm_temp(const adbms_bms_t *bms) {
    return bms->max_therm_temp;
}

/// Get the average thermistor temperature across all modules in Celsius
static inline float ADBMS_get_avg_therm_temp(const adbms_bms_t *bms) {
    return bms->avg_therm_temp;
}

/// Check whether a cell voltage read command (`0 .. ADBMS6380_RDCV_CMD_COUNT - 1`) failed PEC
static inline bool ADBMS_get_cell_voltage_pec_err(const adbms_bms_t *bms, size_t cmd_idx) {
    return bms->dev.err_cell_voltage_pecs[cmd_idx];
}

/// Check whether a GPIO voltage read command (`0 .. ADBMS6380_RDAUX_CMD_COUNT - 1`) failed PEC
static inline bool ADBMS_get_gpio_voltage_pec_err(const adbms_bms_t *bms, size_t cmd_idx) {
    return bms->dev.err_gpio_voltage_pecs[cmd_idx];
}

/// Get the current ADBMS driver state
static inline adbms_state_t ADBMS_get_state(const adbms_bms_t *bms) {
    return bms->state;
}

/// Check whether cell discharge balancing is currently enabled
static inline bool ADBMS_is_balancing_enabled(const adbms_bms_t *bms) {
    return bms->is_balancing_enabled;
}

/// Enable or disable cell discharge balancing
static inline void ADBMS_set_balancing_enabled(adbms_bms_t *bms, bool enabled) {
    bms->is_balancing_enabled = enabled;
}

#endif // ADBMS_H
