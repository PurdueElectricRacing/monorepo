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

#include "common/phal_G4/spi/spi.h"
#include "common/strbuf/strbuf.h"

// Number of cells per ADBMS6380 device.
#define ADBMS6380_CELL_COUNT (16)
// Number of GPIO/aux channels per ADBMS6380 device.
#define ADBMS6380_GPIO_COUNT (10)

// PEC size in bytes for ADBMS6380 commands and data packets. Is 2 bytes for both PEC10 (data) and PEC15 (commands).
#define ADBMS6380_PEC_SIZE (2)
// Raw command size (bytes) for one module without PEC.
#define ADBMS6380_COMMAND_RAW_SIZE (2)
// Command packet size (bytes) for one module including PEC.
#define ADBMS6380_COMMAND_PKT_SIZE (ADBMS6380_COMMAND_RAW_SIZE + ADBMS6380_PEC_SIZE)
// Single data payload size (bytes) for one module without PEC.
#define ADBMS6380_SINGLE_DATA_RAW_SIZE (6)
// Single data packet size (bytes) for one module including PEC.
#define ADBMS6380_SINGLE_DATA_PKT_SIZE (ADBMS6380_SINGLE_DATA_RAW_SIZE + ADBMS6380_PEC_SIZE)

// Number of commands to read all 16 cell voltages (5 for 3 cells each, 1 for the last cell)
// {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF}
#define ADBMS6380_RDCV_CMD_COUNT (6)

// Number of commands to read all 10 GPIO voltages (3 for 3 GPIOs each, 1 for the last GPIO)
// {RDAUXA, RDAUXB, RDAUXC, RDAUXD}
#define ADBMS6380_RDAUX_CMD_COUNT (4)

/**
 * @brief Wake pulse duration per CS toggle in milliseconds.
 * 
 * Note: usage of this expects the FreeRTOS tick rate to be 1000 Hz (1 tick = 1 ms).
 */
#define ADBMS6380_WAKE_DELAY_MS (1)

/**
 * @brief Result of a read operation from the ADBMS6380.
 * 
 * Used to indicate success, PEC failure, or SPI communication failure.
 * Success = 0.
 */
typedef enum {
    ADBMS6380_READ_SUCCESS = 0,
    ADBMS6380_READ_PEC_FAILURE,
    ADBMS6380_READ_SPI_FAILURE,
} adbms6380_read_result_t;

/**
 * @brief Drive the ADBMS CS line low.
 *
 * @param spi SPI configuration containing the CS GPIO.
 */
void adbms6380_set_cs_low(SPI_InitConfig_t *spi);
/**
 * @brief Drive the ADBMS CS line to high.
 *
 * @param spi SPI configuration containing the CS GPIO.
 */
void adbms6380_set_cs_high(SPI_InitConfig_t *spi);

/**
 * @brief Wake a daisy-chained set of ADBMS devices.
 *
 * Toggles CS with blocking delays to bring each module out of sleep.
 *
 * @param spi SPI configuration containing the CS GPIO.
 * @param module_count Number of modules in the daisy chain.
 */
void adbms6380_wake(SPI_InitConfig_t *spi, size_t module_count);

/**
 * @brief Convert a voltage threshold (V) into 12-bit REG_B threshold encoding.
 *
 * @param threshold_voltage Threshold voltage in volts.
 * @return 12-bit configuration value suitable for VOV/VUV fields.
 */
uint16_t adbms6380_get_threshold_voltage_cfg(float threshold_voltage);
/**
 * @brief Extract a little-endian 16-bit signed value from a byte buffer.
 *
 * @param data Pointer to byte buffer containing 16-bit words.
 * @param idx Index of the 16-bit word to extract.
 * @return Signed 16-bit value.
 */
int16_t adbms6380_extract_i16(const uint8_t *data, int idx);
/**
 * @brief Convert raw voltage measurement from the ADBMS6380 to volts.
 *
 * @param raw Raw code from ADBMS.
 * @return Corresponding voltage in volts.
 */
float adbms6380_raw_to_v(int16_t raw);

/**
 * @brief Build ADCV command bytes.
 *
 * @param output_cmd Output buffer for 2-byte command.
 * @param rd Redundant mode.
 * @param cont Continuous conversion enable.
 * @param dcp Discharge permitted during conversion (only affects PWM balancing, not DCC).
 * @param rstf Reset filter.
 * @param ow Open-wire setting (2-bit).
 */
void adbms6380_adcv(uint8_t output_cmd[ADBMS6380_COMMAND_RAW_SIZE],
                    bool rd,
                    bool cont,
                    bool dcp,
                    bool rstf,
                    uint8_t ow);
/**
 * @brief Build ADSV command bytes.
 *
 * @param output_cmd Output buffer for 2-byte command.
 * @param cont Continuous conversion enable.
 * @param dcp Discharge permitted during conversion (only affects PWM balancing, not DCC).
 * @param ow Open-wire setting (2-bit).
 */
void adbms6380_adsv(uint8_t output_cmd[ADBMS6380_COMMAND_RAW_SIZE],
                    bool cont,
                    bool dcp,
                    uint8_t ow);

/**
 * @brief Build ADAX (aux) command bytes.
 * 
 * @param output_cmd Output buffer for 2-byte command.
 * @param ow Open-wire detection enable.
 * @param pup Pull-up enable for open-wire detection.
 * @param ch Channel selection (5-bit). 0 for all.
 */
void adbms6380_adax(uint8_t output_cmd[ADBMS6380_COMMAND_RAW_SIZE], bool ow, bool pup, uint8_t ch);

/**
 * @brief Append a command and its PEC to an output buffer.
 *
 * @param output_buffer Buffer to append into.
 * @param command 2-byte command payload.
 */
void adbms6380_prepare_command(strbuf_t *output_buffer,
                               const uint8_t command[ADBMS6380_COMMAND_RAW_SIZE]);
/**
 * @brief Append a data payload and its PEC to an output buffer.
 *
 * @param output_buffer Buffer to append into.
 * @param data 6-byte data payload.
 */
void adbms6380_prepare_data_packet(strbuf_t *output_buffer,
                                   const uint8_t data[ADBMS6380_SINGLE_DATA_RAW_SIZE]);

/**
 * @brief Calculate the REG_A configuration register value.
 *
 * Populates GPIO pull-downs, reference behavior, and comparator threshold.
 *
 * @param output_cfg_rega Output buffer for REG_A configuration (6 bytes).
 * @param refon 1 = reference remains powered up until watchdog timeout.
 * @param cth C-ADC vs. S-ADC comparison voltage threshold.
 */
void adbms6380_calculate_cfg_rega(uint8_t output_cfg_rega[ADBMS6380_SINGLE_DATA_RAW_SIZE],
                                  bool refon,
                                  uint8_t cth);
/**
 * @brief Calculate the REG_B configuration register value.
 *
 * Encodes VOV/VUV thresholds and discharge control bits.
 *
 * @param output_cfg_regb Output buffer for REG_B configuration (6 bytes).
 * @param overvoltage_threshold Overvoltage threshold in volts.
 * @param undervoltage_threshold Undervoltage threshold in volts.
 * @param is_discharging Per-cell discharge enable flags.
 */
void adbms6380_calculate_cfg_regb(uint8_t output_cfg_regb[ADBMS6380_SINGLE_DATA_RAW_SIZE],
                                  float overvoltage_threshold,
                                  float undervoltage_threshold,
                                  const bool is_discharging[ADBMS6380_CELL_COUNT]);

/**
 * @brief Verify the PEC of received data.
 * 
 * @param rx_bytes Pointer to received bytes from a module. PEC10 must follow the raw
                   data bytes.
 * @param rx_len Length of the recieved data from a module in bytes (including PEC,
 *               so the raw data is actually rx_len - ADBMS6380_PEC_SIZE).
 * @return True if the received PEC matches calculated value, false otherwise.
 */
bool adbms6380_check_data_pec(const uint8_t *rx_bytes, size_t rx_len);

/**
 * @brief Read a fixed-length response from all modules after a command.
 *
 * Sends the command to the daisy chain, then reads @p rx_length bytes into
 * @p rx_buffer. Data is returned in module order (0..N-1).
 *
 * @param spi SPI configuration used for the transfer.
 * @param module_count Number of modules in the daisy chain.
 * @param cmd_buffer Command buffer including PEC.
 * @param rx_buffer Output buffer for received bytes.
 * @param rx_length_per_module Number of bytes expected from each module (including PEC).
 * @return A result code indicating success, PEC failure, or SPI failure.
 */
adbms6380_read_result_t adbms6380_read(SPI_InitConfig_t *spi,
                                       size_t module_count,
                                       const uint8_t cmd_buffer[ADBMS6380_COMMAND_PKT_SIZE],
                                       uint8_t *rx_buffer,
                                       size_t rx_length_per_module);
/**
 * @brief Read a single-data-packet response per module.
 *
 * Convenience wrapper around adbms6380_read() with
 * ADBMS6380_SINGLE_DATA_PKT_SIZE as the expected response length per module.
 *
 * @param spi SPI configuration used for the transfer.
 * @param module_count Number of modules in the daisy chain.
 * @param cmd_buffer Command buffer including PEC.
 * @param rx_buffer Output buffer for received bytes.
 * @return A result code indicating success, PEC failure, or SPI failure.
 */
adbms6380_read_result_t adbms6380_read_data(SPI_InitConfig_t *spi,
                                            size_t module_count,
                                            const uint8_t cmd_buffer[ADBMS6380_COMMAND_PKT_SIZE],
                                            uint8_t *rx_buffer);

/**
 * @brief Read a single-data-packet response per module and retries on PEC failure.
 *
 * Convenience wrapper around adbms6380_read_data() that retries the read up to
 * a fixed number of attempts if a PEC failure is detected.
 *
 * @param spi SPI configuration used for the transfer.
 * @param max_retries Maximum number of read attempts if PEC failures occur.
 * @param module_count Number of modules in the daisy chain.
 * @param cmd_buffer Command buffer including PEC.
 * @param rx_buffer Output buffer for received bytes.
 * @return A result code indicating success, PEC failure, or SPI failure.
 */
adbms6380_read_result_t
adbms6380_read_data_with_retries(SPI_InitConfig_t *spi,
                                 size_t max_retries,
                                 size_t module_count,
                                 const uint8_t cmd_buffer[ADBMS6380_COMMAND_PKT_SIZE],
                                 uint8_t *rx_buffer);

/**
 * @brief Read all cell voltages from each module.
 *
 * Issues the RDCVALL command, converts raw values to volts, and fills
 * the provided per-module cell voltage arrays. If any of the commands fail with
 * a PEC error (even with the retries), the successfully read cell voltages will
 * still be updated but the corresponding PEC error flag will be set.
 *
 * @param spi SPI configuration used for the transfer.
 * @param cmd_buffer Scratch buffer for command + PEC.
 * @param rx_buffer RX buffer for raw bytes.
 * @param cell_voltages Per-module arrays of cell voltages to populate.
 * @param cell_voltages_raw Per-module arrays of raw cell voltage codes to populate.
 * @param pec_error_flags Output array of flags indicating which command (if any)
                          had PEC failures after retries are exhausted.
 * @param module_count Number of modules in the daisy chain.
 * @param max_retries_on_pec_failure Number of attempts to retry a read if a PEC
                                     failure is detected before giving up.
 * @return True on success, false on SPI failure.
 */
bool adbms6380_read_cell_voltages(SPI_InitConfig_t *spi,
                                  strbuf_t *cmd_buffer,
                                  uint8_t *rx_buffer,
                                  float **cell_voltages,
                                  int16_t **cell_voltages_raw,
                                  bool pec_error_flags[ADBMS6380_RDCV_CMD_COUNT],
                                  size_t module_count,
                                  size_t max_retries_on_pec_failure);
/**
 * @brief Read all GPIO/aux voltages from each module.
 *
 * Issues RDAUXA/B/C/D commands, converts raw values to volts, and fills
 * the provided per-module GPIO voltage arrays. If any of the commands fail with
 * a PEC error (even with the retries), the successfully read GPIO voltages will
 * still be updated but the corresponding PEC error flag will be set.
 *
 * @param spi SPI configuration used for the transfer.
 * @param cmd_buffer Scratch buffer for command + PEC.
 * @param rx_buffer RX buffer for raw bytes.
 * @param gpio_voltages Per-module arrays of GPIO voltages to populate.
 * @param pec_error_flags Output array of flags indicating which command (if any)
                          had PEC failures after retries are exhausted.
 * @param module_count Number of modules in the daisy chain.
 * @param max_retries_on_pec_failure Number of attempts to retry a read if a PEC
                                     failure is detected before giving up.
 * @return True on success, false on SPI failure.
 */
bool adbms6380_read_gpio_voltages(SPI_InitConfig_t *spi,
                                  strbuf_t *cmd_buffer,
                                  uint8_t *rx_buffer,
                                  float **gpio_voltages,
                                  bool pec_error_flags[ADBMS6380_RDAUX_CMD_COUNT],
                                  size_t module_count,
                                  size_t max_retries_on_pec_failure);

// Other adbms6380 related function declarations can go here

#endif // ADBMS6380_H
