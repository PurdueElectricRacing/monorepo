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
#include "common/phal/spi.h"
#include "common/strbuf/strbuf.h"

// Number of ADBMS modules in the daisy chain.
#define ADBMS_MODULE_COUNT (8)

// Number of attempts to retry a read if a PEC failure is detected before continuing.
#define ADBMS_PEC_FAIL_MAX_RETRIES (3)

// 10kOhm voltage divider on the thermistor input
#define ADBMS_GPIO_R1 (10000.0f)
// VREF2 = 3.0v and is the input to the thermistor voltage divider
#define ADBMS_GPIO_VIN (3.0f)

// Max SPI TX is a command + all the data packets for all the modules.
#define ADBMS_SPI_TX_BUFFER_SIZE \
    (ADBMS6380_COMMAND_PKT_SIZE + (ADBMS_MODULE_COUNT * ADBMS6380_SINGLE_DATA_PKT_SIZE))
// MAX SPI RX is all the data packets for all the modules.
#define ADBMS_SPI_RX_BUFFER_SIZE (ADBMS6380_SINGLE_DATA_PKT_SIZE * ADBMS_MODULE_COUNT)

#define ADBMS_REGA_REFON        (true)
#define ADBMS_REGA_CTH          (0b110) // 25.05 mV
#define ADBMS_REGB_OV_THRESHOLD (4.2f)  // Volts
#define ADBMS_REGB_UV_THRESHOLD (3.0f)  // Volts
#define ADBMS_ADCV_RD           (false)
#define ADBMS_ADCV_CONT         (true) // Continuous
#define ADBMS_ADCV_DCP          (false)
#define ADBMS_ADCV_RSTF         (true)
#define ADBMS_ADCV_OW           (0b00)
#define ADBMS_ADAX_OW           (false)
#define ADBMS_ADAX_PUP          (false)
#define ADBMS_ADAX_CH           (0b00000)

/**
 * @brief ADBMS driver connection/operation state.
 */
typedef enum {
    ADBMS_STATE_IDLE       = 0, // Not connected or awaiting re-connection.
    ADBMS_STATE_CONNECTED  = 1, // Actively connected and reading/controlling
    ADBMS_STATE_RECOVERING = 2, // Detected an error and attempting to recover
} adbms_state_t;

/**
 * @brief Per-ADBMS module measurements, config, and error flags.
 */
typedef struct {
    /** Latest cell voltage readings for this module (volts). */
    float cell_voltages[ADBMS6380_CELL_COUNT];
    /** Latest raw cell voltage readings for this module. */
    int16_t cell_voltages_raw[ADBMS6380_CELL_COUNT];
    /** Minimum cell voltage within this module (volts). */
    float min_voltage;
    /** Maximum cell voltage within this module (volts). */
    float max_voltage;
    /** Average cell voltage within this module (volts). */
    float avg_voltage;
    /** Sum of cell voltages within this module (volts). */
    float sum_voltage;

    /** Latest thermistor readings as volts. */
    float therms_voltages[ADBMS6380_GPIO_COUNT];
    /** Latest thermistor temperatures in Celsius */
    float therms_temps[ADBMS6380_GPIO_COUNT];

    /** Minimum thermistor temperature within this module (Celsius). */
    float min_therm_temp;
    /** Maximum thermistor temperature within this module (Celsius). */
    float max_therm_temp;
    /** Average thermistor temperature within this module (Celsius). */
    float avg_therm_temp;

    /** Per-cell discharge enable flags used for balancing. Set by BMS, not higher level logic. */
    bool is_discharging[ADBMS6380_CELL_COUNT];

    /** Cached REGA bytes written to the device. Used to compare against read-back data. */
    uint8_t rega[ADBMS6380_SINGLE_DATA_RAW_SIZE];
    /** Cached REGB bytes written to the device. Used to compare against read-back data. */
    uint8_t regb[ADBMS6380_SINGLE_DATA_RAW_SIZE];

    /** Set if a read-back REGA does not match cached REGA. */
    bool err_rega_mismatch;
    /** Set if a read-back REGB does not match cached REGB. */
    bool err_regb_mismatch;
} adbms_module_t;

/**
 * @brief Top-level ADBMS driver state, I/O buffers, measurements, and error flags.
 */
typedef struct {
    /** Current driver state. */
    adbms_state_t state;

    /** Per-module state and measurements. */
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

    /** True if cell discharge balancing is permitted by higher-level logic. */
    bool is_balancing_enabled;

    /** SPI instance used for ADBMS communication (CS control is manual by ADBMS6380 driver). */
    SPI_InitConfig_t *spi;
    /** Scratch TX buffer wrapper for command/data packets. */
    strbuf_t tx_strbuf;
    /** RX buffer for multi-module readouts. */
    uint8_t rx_buf[ADBMS_SPI_RX_BUFFER_SIZE];

    /** Set on any SPI transfer/read failure. Should be a terminal error. */
    bool err_spi;
    /** Set when connect/initialization sequence fails. Retry logic up to higher-level logic. */
    bool err_connect;
    /** Aggregated REGA mismatch flag across modules. */
    bool err_rega_mismatch;
    /** Aggregated REGB mismatch flag across modules. */
    bool err_regb_mismatch;

    /** Set when REGA read has a PEC failure. */
    bool err_rega_pec;
    /** Set when REGB read has a PEC failure. */
    bool err_regb_pec;
    /** Per-command PEC error flags for cell voltage reads. */
    bool err_cell_voltage_pecs[ADBMS6380_RDCV_CMD_COUNT];
    /** Per-command PEC error flags for GPIO voltage reads. */
    bool err_gpio_voltage_pecs[ADBMS6380_RDAUX_CMD_COUNT];
} adbms_bms_t;

/**
 * @brief Initialize the ADBMS driver instance and TX buffer.
 *
 * Sets the driver state to idle, clears discharge flags (and discharged enabled),
 * and sets the provided SPI config and TX buffer into the internal strbuf.
 *
 * @param bms Pointer to driver state to initialize.
 * @param spi SPI configuration used for ADBMS transactions.
 * @param tx_buf Backing buffer for TX command/data packets.
 */
void adbms_init(adbms_bms_t *bms, SPI_InitConfig_t *spi, uint8_t *tx_buf);

/**
 * @brief Calculate and write REGA configuration to all modules.
 *
 * @param bms Pointer to driver state.
 * @return True on successful SPI transfer, false on failure.
 */
bool adbms_write_rega(adbms_bms_t *bms);
/**
 * @brief Calculate and write REGB configuration to all modules.
 *
 * Computes REGB with OV/UV thresholds and discharge flags, then writes
 * the configuration to all modules.
 *
 * @param bms Pointer to driver state.
 * @return True on successful SPI transfer, false on failure.
 */
bool adbms_write_regb(adbms_bms_t *bms);
/**
 * @brief Read back REGA and compare with cached configuration.
 *
 * Sets per-module and aggregated mismatch flags when the read-back does
 * not match the cached REGA data.
 *
 * @param bms Pointer to driver state.
 * @return False on SPI failure, PEC error, or when there is a mismatch; true otherwise.
 */
bool adbms_read_and_check_rega(adbms_bms_t *bms);
/**
 * @brief Read back REGB and compare with cached configuration.
 *
 * Sets per-module and aggregated mismatch flags when the read-back does
 * not match the cached REGB data.
 *
 * @param bms Pointer to driver state.
 * @return False on SPI failure, PEC error, or when there is a mismatch; true otherwise.
 */
bool adbms_read_and_check_regb(adbms_bms_t *bms);

/**
 * @brief Perform the connect/bring-up sequence for all modules.
 *
 * Writes REGA/REGB, verifies read-back, and starts ADCV conversions.
 *
 * @param bms Pointer to driver state.
 */
void adbms_connect(adbms_bms_t *bms);

/**
 * @brief Read all cell voltages and update module/pack statistics.
 *
 * Updates per-module min/max/avg/sum and aggregated min/max/avg/sum for
 * the full pack.
 *
 * @param bms Pointer to driver state.
 */
void adbms_read_cells(adbms_bms_t *bms);
/**
 * @brief Read all GPIO/thermistor values (voltage and calculate temperature)
 * and update module/pack statistics.
 *
 * Updates per-module min/max/avg for thermistor temperatures and aggregated
 * min/max/avg for the full pack.
 *
 * @param bms Pointer to driver state.
 */
void adbms_read_therms(adbms_bms_t *bms);

/**
 * @brief Compute per-cell discharge flags based on pack voltage spread.
 *
 * If balancing is disabled, clears all discharge flags. If the pack
 * minimum voltage is below @p min_voltage, no balancing occurs. Otherwise,
 * discharges cells above (min + delta).
 *
 * @param bms Pointer to driver state.
 * @param min_voltage Minimum pack voltage required to allow balancing.
 * @param min_delta Voltage delta above minimum to start discharging.
 */
void adbms_calculate_balance_cells(adbms_bms_t *bms, float min_voltage, float min_delta);
/**
 * @brief Compute discharge flags and write REGB to apply balancing.
 *
 * Calls adbms_calculate_balance_cells(), writes REGB, and verifies it.
 *
 * @param bms Pointer to driver state.
 * @param min_voltage Minimum pack voltage required to allow balancing.
 * @param min_delta Voltage delta above minimum to start discharging.
 */
void adbms_balance_and_update_regb(adbms_bms_t *bms, float min_voltage, float min_delta);

/**
 * @brief Periodic service routine for connection, measurements, and balancing.
 *
 * If idle, attempts to connect. When connected, reads cells and thermistors,
 * then updates balancing + REGB.
 *
 * @param bms Pointer to driver state.
 * @param min_voltage_for_balance Minimum pack voltage required to allow balancing.
 * @param min_delta_for_balance Voltage delta above minimum to start discharging.
 */
void adbms_periodic(adbms_bms_t *bms, float min_voltage_for_balance, float min_delta_for_balance);

#endif // ADBMS_H