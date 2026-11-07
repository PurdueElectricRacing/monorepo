/**
 * @file adbms6380.c
 * @brief Low level BMS driver specific to ADBMS6380 chip.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "adbms6380.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "commands.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/spi/spi.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "pec.h"
#include "thermistor.h"

// Number of attempts to retry a read if a PEC failure is detected before continuing.
static constexpr size_t ADBMS6380_PEC_FAIL_MAX_RETRIES = 3U;

/**
 * @brief Wake pulse duration per CS toggle in milliseconds.
 *
 * Note: usage of this expects the FreeRTOS tick rate to be 1000 Hz (1 tick = 1 ms).
 */
static constexpr uint32_t ADBMS6380_WAKE_DELAY_MS = 1U;

// 10kOhm voltage divider on the thermistor input.
static constexpr float ADBMS6380_GPIO_R1_OHM       = 10'000.0f;
// VREF2 = 3.0v and is the input to the thermistor voltage divider.
static constexpr float ADBMS6380_GPIO_VIN_V        = 3.0f;
static constexpr float ADBMS6380_DIVIDER_EPSILON_V = 0.00001f;

// ADBMS6380 ADC conversion constants from datasheet:
// Voltage = (raw + 10,000) * 150 uV, where 10,000 codes = 1.5 V offset
static constexpr float ADBMS6380_VOLTAGE_OFFSET_V   = 1.5f;
static constexpr float ADBMS6380_ADC_LSB_V          = 0.000150f;
static constexpr float ADBMS6380_THRESHOLD_SCALE    = 16.0f;
static constexpr int32_t ADBMS6380_RAW_OFFSET_CODES = 10'000;
static constexpr uint8_t ADBMS6380_THRESHOLD_BITS   = 12U;
static constexpr uint16_t ADBMS6380_THRESHOLD_MASK  = 0x0FFFU;

static const uint8_t *const RDCV_CMDS[] = {
    ADBMS6380_CMD_RDCVA,
    ADBMS6380_CMD_RDCVB,
    ADBMS6380_CMD_RDCVC,
    ADBMS6380_CMD_RDCVD,
    ADBMS6380_CMD_RDCVE,
    ADBMS6380_CMD_RDCVF,
};
static_assert(sizeof(RDCV_CMDS) / sizeof(RDCV_CMDS[0]) == ADBMS6380_RDCV_CMD_COUNT);

static const uint8_t *const RDAUX_CMDS[] = {
    ADBMS6380_CMD_RDAUXA,
    ADBMS6380_CMD_RDAUXB,
    ADBMS6380_CMD_RDAUXC,
    ADBMS6380_CMD_RDAUXD,
};
static_assert(sizeof(RDAUX_CMDS) / sizeof(RDAUX_CMDS[0]) == ADBMS6380_RDAUX_CMD_COUNT);

static void set_cs_low(SPI_InitConfig_t *spi) {
    PHAL_GPIO_write(spi->nss_gpio_port, spi->nss_gpio_pin, false);
}

static void set_cs_high(SPI_InitConfig_t *spi) {
    PHAL_GPIO_write(spi->nss_gpio_port, spi->nss_gpio_pin, true);
}

static void transfer_tx(adbms6380_t *dev) {
    set_cs_low(dev->spi);
    PHAL_SPI_transferBlocking(dev->spi, dev->tx_strbuf.data, nullptr, (uint16_t)dev->tx_strbuf.length);
    set_cs_high(dev->spi);
}

static uint16_t get_threshold_voltage_cfg(float threshold_voltage_v) {
    float scaled = (threshold_voltage_v - ADBMS6380_VOLTAGE_OFFSET_V)
        / (ADBMS6380_THRESHOLD_SCALE * ADBMS6380_ADC_LSB_V);
    uint16_t cfg = (uint16_t)(scaled + (float)(2U * (1U << (ADBMS6380_THRESHOLD_BITS - 1U))));
    return cfg & ADBMS6380_THRESHOLD_MASK;
}

static int16_t extract_i16(const uint8_t *data, size_t idx) {
    return (int16_t)((int16_t)(data[idx * 2U + 0U] & 0xFFU)
                     | ((int16_t)(data[idx * 2U + 1U] & 0xFFU) << 8U));
}

static float raw_to_v(int16_t raw) {
    return (float)(raw + ADBMS6380_RAW_OFFSET_CODES) * ADBMS6380_ADC_LSB_V;
}

static void encode_adcv(
    uint8_t output_cmd[ADBMS6380_COMMAND_RAW_SIZE],
    const adbms6380_adcv_config_t *cfg
) {
    // 10-0: 0, 1, RD, CONT, 1, 1, DCP, 0, RSTF, OW[1], OW[0]
    output_cmd[0] = 0x02U | (uint8_t)cfg->rd;
    output_cmd[1] = (uint8_t)(((uint8_t)cfg->cont << 7U) | ((uint8_t)cfg->dcp << 4U)
                              | ((uint8_t)cfg->rstf << 2U) | (cfg->ow & 0x03U) | 0x60U);
}

static void encode_adax(
    uint8_t output_cmd[ADBMS6380_COMMAND_RAW_SIZE],
    const adbms6380_adax_config_t *cfg
) {
    // 10-0: 1 0 OW PUP CH[4] 0 1 CH[3] CH[2] CH[1] CH[0]
    output_cmd[0] = 0b100U | (uint8_t)cfg->ow;
    output_cmd[1] = (uint8_t)(((uint8_t)cfg->pup << 7U) | (((cfg->ch >> 4U) & 1U) << 6U)
                              | (0b01U << 4U) | (cfg->ch & 0b01111U));
}

static void prepare_command(
    strbuf_t *output_buffer,
    const uint8_t command[ADBMS6380_COMMAND_RAW_SIZE]
) {
    strbuf_append(output_buffer, command, ADBMS6380_COMMAND_RAW_SIZE);
    uint16_t pec = PEC_calc_pec15(command, (uint8_t)ADBMS6380_COMMAND_RAW_SIZE);
    const uint8_t pec_bytes[ADBMS6380_PEC_SIZE] = {
        (uint8_t)((pec >> 8U) & 0xFFU),
        (uint8_t)(pec & 0xFFU),
    };
    strbuf_append(output_buffer, pec_bytes, ADBMS6380_PEC_SIZE);
}

static void prepare_data_packet(
    strbuf_t *output_buffer,
    const uint8_t data[ADBMS6380_SINGLE_DATA_RAW_SIZE]
) {
    strbuf_append(output_buffer, data, ADBMS6380_SINGLE_DATA_RAW_SIZE);
    uint16_t pec = PEC_calc_pec10(data, ADBMS6380_SINGLE_DATA_RAW_SIZE, false);
    const uint8_t pec_bytes[ADBMS6380_PEC_SIZE] = {
        (uint8_t)((pec >> 8U) & 0xFFU),
        (uint8_t)(pec & 0xFFU),
    };
    strbuf_append(output_buffer, pec_bytes, ADBMS6380_PEC_SIZE);
}

static void pack_cfg_rega(
    uint8_t output_cfg_rega[ADBMS6380_SINGLE_DATA_RAW_SIZE],
    const adbms6380_rega_config_t *cfg
) {
    memset(output_cfg_rega, 0, ADBMS6380_SINGLE_DATA_RAW_SIZE);

    output_cfg_rega[0] = (uint8_t)(((uint8_t)cfg->refon << 7U) | (cfg->cth & 0x07U));
    // all flags 0
    output_cfg_rega[1] = 0b00000000U;
    // no soak
    output_cfg_rega[2] = 0U;

    // All GPIO pull down off
    output_cfg_rega[3] = 0b11111111U; // GPIOs [8:0] pull down OFF
    output_cfg_rega[4] = 0b00000011U; // GPIOs [10:9] pull down OFF

    // No SNAP_ST MUTE_ST COMM_BK FC
    output_cfg_rega[5] = 0U;
}

static void pack_cfg_regb(
    uint8_t output_cfg_regb[ADBMS6380_SINGLE_DATA_RAW_SIZE],
    const adbms6380_regb_config_t *cfg,
    const bool is_discharging[ADBMS6380_CELL_COUNT]
) {
    memset(output_cfg_regb, 0, ADBMS6380_SINGLE_DATA_RAW_SIZE);

    uint16_t overvoltage_cfg  = get_threshold_voltage_cfg(cfg->overvoltage_threshold_v);
    uint16_t undervoltage_cfg = get_threshold_voltage_cfg(cfg->undervoltage_threshold_v);
    // 12 bits vov
    output_cfg_regb[0] = (uint8_t)(overvoltage_cfg & 0xFFU);
    output_cfg_regb[1] |= (uint8_t)((overvoltage_cfg >> 8U) & 0x0FU);
    // 12 bits vuv
    output_cfg_regb[1] |= (uint8_t)((undervoltage_cfg << 4U) & 0xF0U);
    output_cfg_regb[2] |= (uint8_t)((undervoltage_cfg >> 4U) & 0xFFU);
    // all else default
    output_cfg_regb[3] = 0U;

    // DCC
    output_cfg_regb[4] = 0U;
    output_cfg_regb[5] = 0U;
    for (size_t i = 0U; i < ADBMS6380_CELL_COUNT; i++) {
        if (is_discharging[i]) {
            size_t byte_idx = 4U + (i / 8U);
            size_t bit_idx  = i % 8U;
            output_cfg_regb[byte_idx] |= (uint8_t)(1U << bit_idx);
        }
    }
}

static bool check_data_pec(const uint8_t *rx_bytes, size_t rx_len) {
    size_t raw_data_len     = rx_len - ADBMS6380_PEC_SIZE;
    uint16_t calculated_pec = PEC_calc_pec10(rx_bytes, raw_data_len, true);

    uint8_t pec_msb_packed = rx_bytes[raw_data_len];
    uint8_t pec_lsb        = rx_bytes[raw_data_len + 1U];
    uint16_t received_pec  = (uint16_t)(((uint16_t)(pec_msb_packed & 0x03U) << 8U) | (uint16_t)pec_lsb);

    return calculated_pec == received_pec;
}

static bool read_response(adbms6380_t *dev, size_t rx_length_per_chip) {
    // Send command and get response
    set_cs_low(dev->spi);
    // First send command. Command is passed to all chips in the daisy chain.
    PHAL_SPI_transferBlocking(
        dev->spi,
        dev->tx_strbuf.data,
        nullptr,
        (uint16_t)ADBMS6380_COMMAND_PKT_SIZE
    );
    // Then read data back. Data arrives in receive order: chip 0 ... chip N-1
    size_t total_rx_length = ADBMS_CHIP_COUNT * rx_length_per_chip;
    PHAL_SPI_transferBlocking(dev->spi, nullptr, dev->rx_buf, (uint16_t)total_rx_length);
    set_cs_high(dev->spi);

    // Check PEC for each chip's data packet
    for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
        const uint8_t *chip_data = &dev->rx_buf[chip_idx * rx_length_per_chip];
        if (!check_data_pec(chip_data, rx_length_per_chip)) {
            return false;
        }
    }

    return true;
}

static bool read_data_with_retries(adbms6380_t *dev) {
    for (size_t attempt = 0U; attempt < ADBMS6380_PEC_FAIL_MAX_RETRIES; attempt++) {
        if (read_response(dev, ADBMS6380_SINGLE_DATA_PKT_SIZE)) {
            return true;
        }
        // Retry on PEC failure
    }
    // All retries failed due to PEC errors
    return false;
}

void ADBMS6380_init(adbms6380_t *dev, SPI_InitConfig_t *spi) {
    *dev     = (adbms6380_t){0};
    dev->spi = spi;
    set_cs_high(dev->spi);

    dev->tx_strbuf = (strbuf_t){
        .data    = dev->tx_buf,
        .length  = 0U,
        .max_len = ADBMS6380_SPI_TX_BUFFER_SIZE,
    };
}

void ADBMS6380_wake(adbms6380_t *dev) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        set_cs_low(dev->spi);
        RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS);
        set_cs_high(dev->spi);
        RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS);
    }
}

void ADBMS6380_write_rega(adbms6380_t *dev, const adbms6380_rega_config_t *cfg) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_WRCFGA);
    pack_cfg_rega(dev->rega, cfg);
    for (size_t idx = ADBMS_CHIP_COUNT; idx > 0U; idx--) {
        prepare_data_packet(&dev->tx_strbuf, dev->rega);
    }

    transfer_tx(dev);
}

bool ADBMS6380_verify_rega(adbms6380_t *dev) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_RDCFGA);
    if (!read_data_with_retries(dev)) {
        dev->err_rega_pec = true;
        return false;
    }
    dev->err_rega_pec = false;

    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        const uint8_t *chip_data = &dev->rx_buf[i * ADBMS6380_SINGLE_DATA_PKT_SIZE];
        dev->chips[i].err_rega_mismatch =
            memcmp(chip_data, dev->rega, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0;
    }
    return !ADBMS6380_has_rega_mismatch(dev);
}

void ADBMS6380_write_regb(adbms6380_t *dev, const adbms6380_regb_config_t *cfg) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_WRCFGB);
    for (size_t idx = ADBMS_CHIP_COUNT; idx > 0U; idx--) {
        size_t chip_idx = idx - 1U;
        pack_cfg_regb(dev->chips[chip_idx].regb, cfg, dev->chips[chip_idx].is_discharging);
        prepare_data_packet(&dev->tx_strbuf, dev->chips[chip_idx].regb);
    }

    transfer_tx(dev);
}

bool ADBMS6380_verify_regb(adbms6380_t *dev) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_RDCFGB);
    if (!read_data_with_retries(dev)) {
        dev->err_regb_pec = true;
        return false;
    }
    dev->err_regb_pec = false;

    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        const uint8_t *chip_data = &dev->rx_buf[i * ADBMS6380_SINGLE_DATA_PKT_SIZE];
        dev->chips[i].err_regb_mismatch =
            memcmp(chip_data, dev->chips[i].regb, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0;
    }
    return !ADBMS6380_has_regb_mismatch(dev);
}

bool ADBMS6380_has_rega_mismatch(const adbms6380_t *dev) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        if (dev->chips[i].err_rega_mismatch) {
            return true;
        }
    }
    return false;
}

bool ADBMS6380_has_regb_mismatch(const adbms6380_t *dev) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        if (dev->chips[i].err_regb_mismatch) {
            return true;
        }
    }
    return false;
}

void ADBMS6380_start_adcv(adbms6380_t *dev, const adbms6380_adcv_config_t *cfg) {
    strbuf_clear(&dev->tx_strbuf);
    uint8_t adcv_cmd[ADBMS6380_COMMAND_RAW_SIZE] = {0};
    encode_adcv(adcv_cmd, cfg);
    prepare_command(&dev->tx_strbuf, adcv_cmd);
    transfer_tx(dev);
}

void ADBMS6380_start_adax(adbms6380_t *dev, const adbms6380_adax_config_t *cfg) {
    strbuf_clear(&dev->tx_strbuf);
    uint8_t adax_cmd[ADBMS6380_COMMAND_RAW_SIZE] = {0};
    encode_adax(adax_cmd, cfg);
    prepare_command(&dev->tx_strbuf, adax_cmd);
    transfer_tx(dev);
}

void ADBMS6380_read_cell_voltages(adbms6380_t *dev) {
    for (size_t cmd_idx = 0U; cmd_idx < countof(RDCV_CMDS); cmd_idx++) {
        strbuf_clear(&dev->tx_strbuf);
        prepare_command(&dev->tx_strbuf, RDCV_CMDS[cmd_idx]);

        if (!read_data_with_retries(dev)) {
            // Skip to the next command, marking that values were not updated for this set of cells
            dev->err_cell_voltage_pecs[cmd_idx] = true;
            continue;
        }
        dev->err_cell_voltage_pecs[cmd_idx] = false;

        // First 5 commands read 3 cells, last reads 1 cell = 16 total
        size_t cells_read            = (cmd_idx < 5U) ? 3U : 1U;
        size_t cell_in_chip_idx_base = cmd_idx * 3U;

        // Data comes back in receive order: chip 0, chip 1, ..., chip N-1
        for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
            const uint8_t *chip_data = &dev->rx_buf[chip_idx * ADBMS6380_SINGLE_DATA_PKT_SIZE];
            for (size_t j = 0U; j < cells_read; j++) {
                size_t cell_idx = cell_in_chip_idx_base + j;
                if (cell_idx >= ADBMS6380_CELL_COUNT) {
                    break; // safety check
                }
                int16_t raw = extract_i16(chip_data, j);
                dev->chips[chip_idx].cell_voltages[cell_idx] = raw_to_v(raw);
            }
        }
    }
}

void ADBMS6380_read_gpio_voltages(adbms6380_t *dev) {
    for (size_t cmd_idx = 0U; cmd_idx < countof(RDAUX_CMDS); cmd_idx++) {
        strbuf_clear(&dev->tx_strbuf);
        prepare_command(&dev->tx_strbuf, RDAUX_CMDS[cmd_idx]);

        if (!read_data_with_retries(dev)) {
            // Skip to the next command, marking that values were not updated for this set of GPIOs
            dev->err_gpio_voltage_pecs[cmd_idx] = true;
            continue;
        }
        dev->err_gpio_voltage_pecs[cmd_idx] = false;

        // AUXA/B/C: 3 GPIOs each, AUXD: GPIO10
        size_t gpios_read    = (cmd_idx < 3U) ? 3U : 1U;
        size_t gpio_idx_base = cmd_idx * 3U;

        // Data comes back in receive order: chip 0, chip 1, ..., chip N-1
        for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
            const uint8_t *chip_data = &dev->rx_buf[chip_idx * ADBMS6380_SINGLE_DATA_PKT_SIZE];
            for (size_t j = 0U; j < gpios_read; j++) {
                size_t gpio_idx = gpio_idx_base + j;
                int16_t raw     = extract_i16(chip_data, j);
                dev->chips[chip_idx].gpio_voltages[gpio_idx] = raw_to_v(raw);
            }
        }
    }
}

static float gpio_v_to_r2(float v_out) {
    // Avoid divide by 0:
    if (v_out == ADBMS6380_GPIO_VIN_V) {
        v_out = ADBMS6380_GPIO_VIN_V - ADBMS6380_DIVIDER_EPSILON_V;
    }
    return (v_out * ADBMS6380_GPIO_R1_OHM) / (ADBMS6380_GPIO_VIN_V - v_out);
}

void ADBMS6380_read_temperatures(adbms6380_t *dev, const adbms6380_adax_config_t *cfg) {
    ADBMS6380_start_adax(dev, cfg);

    // TODO: wait for AUX conversion completion before reading (PLAUX)
    // ADAX preserves previous results, so this immediate read may return the
    // previous scan or mixed old/new values.
    // This is not terrible, but should be fixed later.
    ADBMS6380_read_gpio_voltages(dev);

    // Convert all GPIO voltages to temperatures
    // GPIO voltage -> R2 in the thermistor divider -> temperature via thermistor curve
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS6380_GPIO_COUNT; j++) {
            float v_out = dev->chips[i].gpio_voltages[j];
            // Avoid divide by 0:
            if (v_out == ADBMS6380_GPIO_VIN_V) {
                v_out = ADBMS6380_GPIO_VIN_V - ADBMS6380_DIVIDER_EPSILON_V;
            }
            float r2                      = (v_out * ADBMS6380_GPIO_R1_OHM) / (ADBMS6380_GPIO_VIN_V - v_out);
            dev->chips[i].therms_temps[j] = thermistor_R_to_T(r2);
        }
    }
}
