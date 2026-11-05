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
#include "common/phal_G4/spi/spi.h"

// Number of ADBMS modules in the daisy chain
static constexpr size_t ADBMS_MODULE_COUNT = ADBMS6380_MODULE_COUNT;

// 10kOhm voltage divider on the thermistor input
static constexpr float ADBMS_GPIO_R1_OHM = 10'000.0f;
// VREF2 = 3.0v and is the input to the thermistor voltage divider
static constexpr float ADBMS_GPIO_VIN_V = 3.0f;

// Wait longer than the ADBMS6380 ~2.2 s inactivity sleep timeout so that a
// full sleep-wake cycle elapses during recovery before we retry connecting
static constexpr uint32_t ADBMS_CHIP_SLEEP_CYCLE_MS = 2'500U;

// Number of consecutive failed recovery cycles (each a full sleep-wake
// attempt) before the driver enters the latching FATAL state
static constexpr uint8_t ADBMS_RECOVERY_MAX_ATTEMPTS = 4U;

// Readings outside these are treated as corrupted data rather than real measurements
// and causes a transition to RECOVERING.
static constexpr float ADBMS_CELL_MIN_VALID_V = 1.0f;
static constexpr float ADBMS_CELL_MAX_VALID_V = 5.0f;

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
 * @brief Per-ADBMS module derived temperatures and aggregate statistics.
 */
typedef struct {
    /** Latest thermistor temperatures in Celsius */
    float therms_temps[ADBMS6380_GPIO_COUNT];

    /** Minimum cell voltage within this module (volts). */
    float min_voltage;
    /** Maximum cell voltage within this module (volts). */
    float max_voltage;
    /** Average cell voltage within this module (volts). */
    float avg_voltage;
    /** Sum of cell voltages within this module (volts). */
    float sum_voltage;

    /** Minimum thermistor temperature within this module (Celsius). */
    float min_therm_temp;
    /** Maximum thermistor temperature within this module (Celsius). */
    float max_therm_temp;
    /** Average thermistor temperature within this module (Celsius). */
    float avg_therm_temp;
} adbms_module_t;

/**
 * @brief Top-level ADBMS driver state, device handle, and pack-level statistics.
 */
typedef struct {
    /** Low-level ADBMS6380 daisy-chain device state, SPI buffers, and error flags. */
    adbms6380_t dev;
    /** Per-module derived temperatures and aggregate statistics. */
    adbms_module_t modules[ADBMS_MODULE_COUNT];

    /** Minimum cell voltage across all modules (volts). */
    float min_voltage;
    /** Maximum cell voltage across all modules (volts). */
    float max_voltage;
    /** Average cell voltage across all modules (volts). */
    float avg_voltage;
    /** Sum of cell voltages across all modules (volts). */
    float sum_voltage;

    /** Minimum thermistor temperature across all modules (Celsius). */
    float min_therm_temp;
    /** Maximum thermistor temperature across all modules (Celsius). */
    float max_therm_temp;
    /** Average thermistor temperature across all modules (Celsius). */
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

#endif // ADBMS_H
