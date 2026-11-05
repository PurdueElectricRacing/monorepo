/**
 * @file adbms6380.h
 * @brief Low level BMS driver specific to ADBMS6380 chip.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef ADBMS6380_H
#define ADBMS6380_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adbms_config.h"
#include "common/phal_G4/spi/spi.h"
#include "common/strbuf/strbuf.h"

// Number of cells per ADBMS6380 device.
static constexpr size_t ADBMS6380_CELL_COUNT = 16U;
// Number of GPIO/aux channels per ADBMS6380 device.
static constexpr size_t ADBMS6380_GPIO_COUNT = 10U;

// PEC size in bytes for ADBMS6380 commands and data packets. Is 2 bytes for both PEC10 (data) and PEC15 (commands).
static constexpr size_t ADBMS6380_PEC_SIZE = 2U;
// Raw command size (bytes) for one module without PEC.
static constexpr size_t ADBMS6380_COMMAND_RAW_SIZE = 2U;
// Command packet size (bytes) for one module including PEC.
static constexpr size_t ADBMS6380_COMMAND_PKT_SIZE = ADBMS6380_COMMAND_RAW_SIZE + ADBMS6380_PEC_SIZE;
// Single data payload size (bytes) for one module without PEC.
static constexpr size_t ADBMS6380_SINGLE_DATA_RAW_SIZE = 6U;
// Single data packet size (bytes) for one module including PEC.
static constexpr size_t ADBMS6380_SINGLE_DATA_PKT_SIZE =
    ADBMS6380_SINGLE_DATA_RAW_SIZE + ADBMS6380_PEC_SIZE;

// Number of commands to read all 16 cell voltages (5 for 3 cells each, 1 for the last cell)
// {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF}
static constexpr size_t ADBMS6380_RDCV_CMD_COUNT = 6U;

// Number of commands to read all 10 GPIO voltages (3 for 3 GPIOs each, 1 for the last GPIO)
// {RDAUXA, RDAUXB, RDAUXC, RDAUXD}
static constexpr size_t ADBMS6380_RDAUX_CMD_COUNT = 4U;

// Max SPI TX is a command + all the data packets for all the modules.
static constexpr size_t ADBMS6380_SPI_TX_BUFFER_SIZE =
    ADBMS6380_COMMAND_PKT_SIZE + (ADBMS_MODULE_COUNT * ADBMS6380_SINGLE_DATA_PKT_SIZE);
// Max SPI RX is all the data packets for all the modules.
static constexpr size_t ADBMS6380_SPI_RX_BUFFER_SIZE =
    ADBMS6380_SINGLE_DATA_PKT_SIZE * ADBMS_MODULE_COUNT;

/**
 * @brief Configuration parameters for the CFGA register group.
 */
typedef struct {
    /** C-ADC vs. S-ADC comparison voltage threshold (3-bit). */
    uint8_t cth;
    /** true = reference remains powered up until watchdog timeout. */
    bool refon;
} adbms6380_rega_config_t;

/**
 * @brief Configuration parameters for the CFGB register group.
 */
typedef struct {
    /** Overvoltage threshold in volts. */
    float overvoltage_threshold_v;
    /** Undervoltage threshold in volts. */
    float undervoltage_threshold_v;
} adbms6380_regb_config_t;

/**
 * @brief Configuration parameters for the ADCV command.
 */
typedef struct {
    /** Open-wire setting (2-bit). */
    uint8_t ow;
    /** Redundant mode. */
    bool rd;
    /** Continuous conversion enable. */
    bool cont;
    /** Discharge permitted during conversion (only affects PWM balancing, not DCC). */
    bool dcp;
    /** Reset filter. */
    bool rstf;
} adbms6380_adcv_config_t;

/**
 * @brief Configuration parameters for the ADAX (aux) command.
 */
typedef struct {
    /** Channel selection (5-bit). 0 for all. */
    uint8_t ch;
    /** Open-wire detection enable. */
    bool ow;
    /** Pull-up enable for open-wire detection. */
    bool pup;
} adbms6380_adax_config_t;

/**
 * @brief Per-module device-level measurements, register cache, and mismatch flags.
 */
typedef struct {
    /** Latest cell voltage readings for this module (volts). */
    float cell_voltages[ADBMS6380_CELL_COUNT];
    /** Latest GPIO/aux voltage readings for this module (volts). */
    float gpio_voltages[ADBMS6380_GPIO_COUNT];
    /** Latest thermistor temperatures for this module (Celsius). */
    float therms_temps[ADBMS6380_GPIO_COUNT];
    /** Cached REGB bytes written to the device. Used to compare against read-back data. */
    uint8_t regb[ADBMS6380_SINGLE_DATA_RAW_SIZE];
    /** Per-cell discharge enable flags used for balancing. */
    bool is_discharging[ADBMS6380_CELL_COUNT];
    /** Set if a read-back REGA does not match cached REGA. */
    bool err_rega_mismatch;
    /** Set if a read-back REGB does not match cached REGB. */
    bool err_regb_mismatch;
} adbms6380_module_t;

/**
 * @brief Daisy-chain ADBMS6380 device driver state, SPI transport, buffers, and error flags.
 */
typedef struct {
    /** SPI instance used for ADBMS communication (CS control is manual by ADBMS6380 driver). */
    SPI_InitConfig_t *spi;
    /** Scratch TX buffer wrapper for command/data packets. */
    strbuf_t tx_strbuf;
    /** Per-module device state and raw/converted voltage and temperature readings. */
    adbms6380_module_t modules[ADBMS_MODULE_COUNT];
    /** Backing TX buffer for command/data packets. */
    uint8_t tx_buf[ADBMS6380_SPI_TX_BUFFER_SIZE];
    /** RX buffer for multi-module readouts. */
    uint8_t rx_buf[ADBMS6380_SPI_RX_BUFFER_SIZE];
    /** Cached REGA bytes written to all modules in the device chain. */
    uint8_t rega[ADBMS6380_SINGLE_DATA_RAW_SIZE];
    /** Per-command PEC error flags for cell voltage reads. */
    bool err_cell_voltage_pecs[ADBMS6380_RDCV_CMD_COUNT];
    /** Per-command PEC error flags for GPIO voltage reads. */
    bool err_gpio_voltage_pecs[ADBMS6380_RDAUX_CMD_COUNT];
    /** Set when REGA read has a PEC failure. */
    bool err_rega_pec;
    /** Set when REGB read has a PEC failure. */
    bool err_regb_pec;
} adbms6380_t;

/**
 * @brief Initialize the ADBMS6380 device driver instance, SPI buffers, and CS line.
 *
 * Deasserts CS high, initializes the internal TX strbuf, and clears discharge
 * and error flags across all modules.
 *
 * @param dev Non-null pointer to device state to initialize.
 * @param spi Non-null SPI configuration used for ADBMS transactions.
 */
void ADBMS6380_init(adbms6380_t *dev, SPI_InitConfig_t *spi);

/**
 * @brief Wake the daisy-chained set of ADBMS6380 devices.
 *
 * Toggles CS with blocking delays to bring each module out of sleep.
 *
 * @param dev Non-null pointer to device state.
 */
void ADBMS6380_wake(adbms6380_t *dev);

/**
 * @brief Encode and write REGA configuration to all modules in the daisy chain.
 *
 * @param dev Non-null pointer to device state.
 * @param cfg Non-null pointer to REGA configuration parameters.
 */
void ADBMS6380_write_rega(adbms6380_t *dev, const adbms6380_rega_config_t *cfg);

/**
 * @brief Read back REGA from all modules and compare with cached configuration.
 *
 * Sets per-module mismatch or PEC flags when the read-back fails or does not
 * match the cached REGA data.
 *
 * @param dev Non-null pointer to device state.
 * @return false on PEC error or when there is a mismatch; true otherwise.
 */
bool ADBMS6380_verify_rega(adbms6380_t *dev);

/**
 * @brief Encode and write REGB configuration (OV/UV thresholds + discharge bits) to all modules.
 *
 * @param dev Non-null pointer to device state.
 * @param cfg Non-null pointer to REGB configuration parameters.
 */
void ADBMS6380_write_regb(adbms6380_t *dev, const adbms6380_regb_config_t *cfg);

/**
 * @brief Read back REGB from all modules and compare with cached configuration.
 *
 * Sets per-module mismatch or PEC flags when the read-back fails or does not
 * match the cached REGB data.
 *
 * @param dev Non-null pointer to device state.
 * @return false on PEC error or when there is a mismatch; true otherwise.
 */
bool ADBMS6380_verify_regb(adbms6380_t *dev);

/**
 * @brief Check whether any module in the chain has a REGA mismatch error.
 *
 * @param dev Non-null pointer to device state.
 * @return true if any module has @ref adbms6380_module_t::err_rega_mismatch set.
 */
bool ADBMS6380_has_rega_mismatch(const adbms6380_t *dev);

/**
 * @brief Check whether any module in the chain has a REGB mismatch error.
 *
 * @param dev Non-null pointer to device state.
 * @return true if any module has @ref adbms6380_module_t::err_regb_mismatch set.
 */
bool ADBMS6380_has_regb_mismatch(const adbms6380_t *dev);

/**
 * @brief Issue the ADCV command to start cell voltage conversions across all modules.
 *
 * @param dev Non-null pointer to device state.
 * @param cfg Non-null pointer to ADCV command parameters.
 */
void ADBMS6380_start_adcv(adbms6380_t *dev, const adbms6380_adcv_config_t *cfg);

/**
 * @brief Issue the ADAX command to start GPIO/aux voltage conversions across all modules.
 *
 * @param dev Non-null pointer to device state.
 * @param cfg Non-null pointer to ADAX command parameters.
 */
void ADBMS6380_start_adax(adbms6380_t *dev, const adbms6380_adax_config_t *cfg);

/**
 * @brief Read all cell voltages from each module via RDCVA..RDCVF.
 *
 * Converts raw values to volts and populates each module's @ref cell_voltages array.
 * If any command fails with a PEC error after retries, the successfully read
 * cell voltages are still updated and the corresponding flag in
 * @ref err_cell_voltage_pecs is set.
 *
 * @param dev Non-null pointer to device state.
 */
void ADBMS6380_read_cell_voltages(adbms6380_t *dev);

/**
 * @brief Read all GPIO/aux voltages from each module via RDAUXA..RDAUXD.
 *
 * Converts raw values to volts and populates each module's @ref gpio_voltages array.
 * If any command fails with a PEC error after retries, the successfully read
 * GPIO voltages are still updated and the corresponding flag in
 * @ref err_gpio_voltage_pecs is set.
 *
 * @param dev Non-null pointer to device state.
 */
void ADBMS6380_read_gpio_voltages(adbms6380_t *dev);

/**
 * @brief Start an ADAX conversion, read all GPIO voltages, and convert to thermistor temperatures.
 *
 * Populates each module's @ref gpio_voltages and @ref therms_temps arrays.
 *
 * @param dev Non-null pointer to device state.
 * @param cfg Non-null pointer to ADAX command parameters.
 */
void ADBMS6380_read_temperatures(adbms6380_t *dev, const adbms6380_adax_config_t *cfg);

#endif // ADBMS6380_H
