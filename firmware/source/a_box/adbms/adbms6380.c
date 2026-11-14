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
#include "phal_G4/gpio/gpio.h"
#include "phal_G4/spi/spi.h"
#include "rtos/rtos.h"
#include "utils/countof.h"
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

// Loop iterations for >10 us isoSPI port reversal blocking window (tBLOCK in ADBMS6830B datasheet Table 12).
// At 170 MHz max SYSCLK on STM32G474, 3400 NOPs is ~20 us.
static constexpr uint32_t ADBMS6380_TBLOCK_NOP_COUNT = 3'400U;

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

typedef void (*extract_slot_fn_t)(
    adbms6380_t *dev,
    const uint8_t *slot_data,
    size_t chip_idx,
    size_t cmd_idx
);

static void set_cs_low(SPI_InitConfig_t *spi) {
    PHAL_GPIO_write(spi->nss_gpio_port, spi->nss_gpio_pin, false);
}

static void set_cs_high(SPI_InitConfig_t *spi) {
    PHAL_GPIO_write(spi->nss_gpio_port, spi->nss_gpio_pin, true);
}

static SPI_InitConfig_t *get_spi(const adbms6380_t *dev, adbms6380_dir_t dir) {
    return (dir == ADBMS6380_DIR_FWD) ? dev->spi_fwd : dev->spi_rev;
}

static adbms6380_dir_t get_opposite_dir(adbms6380_dir_t dir) {
    return (dir == ADBMS6380_DIR_FWD) ? ADBMS6380_DIR_REV : ADBMS6380_DIR_FWD;
}

/**
 * @brief Map a daisy-chain response slot index (0 = closest to active SPI port)
 * to the physical chip index (0 = closest to SPI1 / Port A, N-1 = closest to SPI2 / Port B).
 */
static size_t map_slot_to_chip_idx(adbms6380_dir_t dir, size_t slot_idx) {
    return (dir == ADBMS6380_DIR_FWD) ? slot_idx : ((ADBMS_CHIP_COUNT - 1U) - slot_idx);
}

/**
 * @brief Wait > tBLOCK (10 us max per ADBMS6830B datasheet) before reversing isoSPI port direction.
 */
static void delay_tblock(void) {
    for (volatile uint32_t i = 0U; i < ADBMS6380_TBLOCK_NOP_COUNT; i++) {
        __asm__ volatile("nop");
    }
}

static void wake_dir(adbms6380_t *dev, adbms6380_dir_t dir, size_t pulse_count) {
    SPI_InitConfig_t *spi = get_spi(dev, dir);
    for (size_t i = 0U; i < pulse_count; i++) {
        set_cs_low(spi);
        RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS);
        set_cs_high(spi);
        RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS);
    }
}

static void transfer_tx_dir(adbms6380_t *dev, adbms6380_dir_t dir) {
    SPI_InitConfig_t *spi = get_spi(dev, dir);
    set_cs_low(spi);
    PHAL_SPI_transferBlocking(spi, dev->tx_strbuf.data, nullptr, (uint16_t)dev->tx_strbuf.length);
    set_cs_high(spi);
}

static void broadcast_cmd(adbms6380_t *dev) {
    if (!dev->break_detected) {
        transfer_tx_dir(dev, dev->primary_dir);
        return;
    }

    if (dev->reachable_fwd > 0U) {
        transfer_tx_dir(dev, ADBMS6380_DIR_FWD);
    }
    if (dev->reachable_rev > 0U) {
        if (dev->reachable_fwd > 0U) {
            delay_tblock();
        }
        transfer_tx_dir(dev, ADBMS6380_DIR_REV);
    }
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

/**
 * @brief Send the prepared command in dev->tx_strbuf on @p dir, clock back all
 * ADBMS_CHIP_COUNT response slots into dev->rx_buf, and return the count of
 * contiguous slots from slot 0 that pass PEC10.
 */
static size_t read_response_dir(
    adbms6380_t *dev,
    adbms6380_dir_t dir,
    size_t rx_length_per_chip
) {
    SPI_InitConfig_t *spi = get_spi(dev, dir);
    set_cs_low(spi);
    PHAL_SPI_transferBlocking(
        spi,
        dev->tx_strbuf.data,
        nullptr,
        (uint16_t)ADBMS6380_COMMAND_PKT_SIZE
    );
    size_t total_rx_length = ADBMS_CHIP_COUNT * rx_length_per_chip;
    PHAL_SPI_transferBlocking(spi, nullptr, dev->rx_buf, (uint16_t)total_rx_length);
    set_cs_high(spi);

    size_t valid_prefix = 0U;
    for (size_t slot_idx = 0U; slot_idx < ADBMS_CHIP_COUNT; slot_idx++) {
        const uint8_t *slot_data = &dev->rx_buf[slot_idx * rx_length_per_chip];
        if (!check_data_pec(slot_data, rx_length_per_chip)) {
            break;
        }
        valid_prefix++;
    }

    return valid_prefix;
}

/**
 * @brief Read response packets on @p dir with retries until at least @p expected_count
 * contiguous chips pass PEC10 (or retries are exhausted).
 */
static size_t read_data_dir_with_retries(
    adbms6380_t *dev,
    adbms6380_dir_t dir,
    size_t expected_count
) {
    size_t valid_count = 0U;
    for (size_t attempt = 0U; attempt < ADBMS6380_PEC_FAIL_MAX_RETRIES; attempt++) {
        valid_count = read_response_dir(dev, dir, ADBMS6380_SINGLE_DATA_PKT_SIZE);
        if (valid_count >= expected_count) {
            return valid_count;
        }
    }
    return valid_count;
}

/**
 * @brief Update isoSPI break tracking state from the measured forward and reverse valid chip counts.
 *
 * @return true if all ADBMS_CHIP_COUNT chips in the chain were reached across FWD + REV;
 *         false if one or more chips are unreachable from both ends.
 */
static bool update_break_state(adbms6380_t *dev, size_t valid_fwd, size_t valid_rev) {
    if (valid_fwd == ADBMS_CHIP_COUNT && valid_rev == ADBMS_CHIP_COUNT) {
        dev->break_detected = false;
        dev->break_index    = ADBMS6380_NO_BREAK_INDEX;
        dev->reachable_fwd  = (uint8_t)ADBMS_CHIP_COUNT;
        dev->reachable_rev  = (uint8_t)ADBMS_CHIP_COUNT;
        return true;
    }

    if (valid_fwd + valid_rev >= ADBMS_CHIP_COUNT) {
        uint8_t split_idx   = (uint8_t)valid_fwd;
        dev->break_detected = true;
        dev->break_index    = split_idx;
        dev->reachable_fwd  = split_idx;
        dev->reachable_rev  = (uint8_t)(ADBMS_CHIP_COUNT - split_idx);
        return true;
    }

    dev->break_detected = true;
    dev->break_index    = (uint8_t)valid_fwd;
    dev->reachable_fwd  = (uint8_t)valid_fwd;
    dev->reachable_rev  = (uint8_t)valid_rev;
    return false;
}

static void write_rega_dir(adbms6380_t *dev, adbms6380_dir_t dir) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_WRCFGA);
    for (size_t idx = 0U; idx < ADBMS_CHIP_COUNT; idx++) {
        prepare_data_packet(&dev->tx_strbuf, dev->rega);
    }
    transfer_tx_dir(dev, dir);
}

static void write_regb_dir(adbms6380_t *dev, adbms6380_dir_t dir) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, ADBMS6380_CMD_WRCFGB);
    if (dir == ADBMS6380_DIR_FWD) {
        // Forward (SPI1): last packet sent latches into Chip 0 (closest to SPI1)
        for (size_t idx = ADBMS_CHIP_COUNT; idx > 0U; idx--) {
            size_t chip_idx = idx - 1U;
            prepare_data_packet(&dev->tx_strbuf, dev->chips[chip_idx].regb);
        }
    } else {
        // Reverse (SPI2): last packet sent latches into Chip N-1 (closest to SPI2)
        for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
            prepare_data_packet(&dev->tx_strbuf, dev->chips[chip_idx].regb);
        }
    }
    transfer_tx_dir(dev, dir);
}

static void extract_rega_verify_slot(
    adbms6380_t *dev,
    const uint8_t *slot_data,
    size_t chip_idx,
    size_t cmd_idx
) {
    (void)cmd_idx;
    dev->chips[chip_idx].err_rega_mismatch =
        memcmp(slot_data, dev->rega, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0;
}

static void extract_regb_verify_slot(
    adbms6380_t *dev,
    const uint8_t *slot_data,
    size_t chip_idx,
    size_t cmd_idx
) {
    (void)cmd_idx;
    dev->chips[chip_idx].err_regb_mismatch =
        memcmp(slot_data, dev->chips[chip_idx].regb, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0;
}

static void extract_cell_voltage_slot(
    adbms6380_t *dev,
    const uint8_t *slot_data,
    size_t chip_idx,
    size_t cmd_idx
) {
    // First 5 commands read 3 cells, last reads 1 cell = 16 total
    size_t cells_read            = (cmd_idx < 5U) ? 3U : 1U;
    size_t cell_in_chip_idx_base = cmd_idx * 3U;

    for (size_t j = 0U; j < cells_read; j++) {
        size_t cell_idx = cell_in_chip_idx_base + j;
        if (cell_idx >= ADBMS6380_CELL_COUNT) {
            break;
        }
        int16_t raw = extract_i16(slot_data, j);
        dev->chips[chip_idx].cell_voltages[cell_idx] = raw_to_v(raw);
    }
}

static void extract_gpio_voltage_slot(
    adbms6380_t *dev,
    const uint8_t *slot_data,
    size_t chip_idx,
    size_t cmd_idx
) {
    // AUXA/B/C: 3 GPIOs each, AUXD: GPIO10
    size_t gpios_read    = (cmd_idx < 3U) ? 3U : 1U;
    size_t gpio_idx_base = cmd_idx * 3U;

    for (size_t j = 0U; j < gpios_read; j++) {
        size_t gpio_idx = gpio_idx_base + j;
        int16_t raw     = extract_i16(slot_data, j);
        dev->chips[chip_idx].gpio_voltages[gpio_idx] = raw_to_v(raw);
    }
}

static void extract_valid_slots(
    adbms6380_t *dev,
    adbms6380_dir_t dir,
    size_t slot_count,
    extract_slot_fn_t extract_fn,
    size_t cmd_idx
) {
    for (size_t slot_idx = 0U; slot_idx < slot_count; slot_idx++) {
        const uint8_t *slot_data = &dev->rx_buf[slot_idx * ADBMS6380_SINGLE_DATA_PKT_SIZE];
        size_t chip_idx          = map_slot_to_chip_idx(dir, slot_idx);
        extract_fn(dev, slot_data, chip_idx, cmd_idx);
    }
}

/**
 * @brief Execute a register-group read command using the active dual-direction strategy:
 * - When intact (!break_detected): reads full chain on primary_dir; if a break is encountered
 *   (valid_pri < N), immediately wakes the opposite port and reads the remaining chips in the same cycle.
 * - When split (break_detected): reads reachable_fwd chips on FWD and reachable_rev chips on REV.
 */
static bool read_register_group(
    adbms6380_t *dev,
    const uint8_t command[ADBMS6380_COMMAND_RAW_SIZE],
    extract_slot_fn_t extract_fn,
    size_t cmd_idx,
    void (*recover_opp_write_fn)(adbms6380_t *dev, adbms6380_dir_t dir),
    bool probe_both_when_split
) {
    strbuf_clear(&dev->tx_strbuf);
    prepare_command(&dev->tx_strbuf, command);

    if (!dev->break_detected) {
        adbms6380_dir_t pri_dir = dev->primary_dir;
        size_t valid_pri        = read_data_dir_with_retries(dev, pri_dir, ADBMS_CHIP_COUNT);
        extract_valid_slots(dev, pri_dir, valid_pri, extract_fn, cmd_idx);

        if (valid_pri == ADBMS_CHIP_COUNT) {
            return true;
        }

        // Break or fault encountered on primary_dir: immediately fall back to opposite port
        adbms6380_dir_t opp_dir = get_opposite_dir(pri_dir);
        size_t remaining        = ADBMS_CHIP_COUNT - valid_pri;
        delay_tblock();
        wake_dir(dev, opp_dir, remaining);

        if (recover_opp_write_fn != nullptr) {
            recover_opp_write_fn(dev, opp_dir);
            strbuf_clear(&dev->tx_strbuf);
            prepare_command(&dev->tx_strbuf, command);
        }

        size_t valid_opp = read_data_dir_with_retries(dev, opp_dir, remaining);
        size_t extract_opp = (valid_opp > remaining) ? remaining : valid_opp;
        extract_valid_slots(dev, opp_dir, extract_opp, extract_fn, cmd_idx);

        size_t valid_fwd = (pri_dir == ADBMS6380_DIR_FWD) ? valid_pri : valid_opp;
        size_t valid_rev = (pri_dir == ADBMS6380_DIR_REV) ? valid_pri : valid_opp;
        return update_break_state(dev, valid_fwd, valid_rev);
    }

    // Already in Split Mode
    size_t valid_fwd = 0U;
    size_t valid_rev = 0U;

    if (dev->reachable_fwd > 0U || probe_both_when_split) {
        valid_fwd = read_data_dir_with_retries(dev, ADBMS6380_DIR_FWD, dev->reachable_fwd);
        extract_valid_slots(dev, ADBMS6380_DIR_FWD, valid_fwd, extract_fn, cmd_idx);
    }

    if (dev->reachable_rev > 0U || probe_both_when_split) {
        if (dev->reachable_fwd > 0U || probe_both_when_split) {
            delay_tblock();
        }
        valid_rev = read_data_dir_with_retries(dev, ADBMS6380_DIR_REV, dev->reachable_rev);
        size_t max_rev_extract = (valid_fwd < ADBMS_CHIP_COUNT)
            ? (ADBMS_CHIP_COUNT - valid_fwd)
            : 0U;
        size_t extract_rev = (valid_rev > max_rev_extract) ? max_rev_extract : valid_rev;
        extract_valid_slots(dev, ADBMS6380_DIR_REV, extract_rev, extract_fn, cmd_idx);
    }

    return update_break_state(dev, valid_fwd, valid_rev);
}

void ADBMS6380_init(adbms6380_t *dev, SPI_InitConfig_t *spi_fwd, SPI_InitConfig_t *spi_rev) {
    *dev         = (adbms6380_t){0};
    dev->spi_fwd = spi_fwd;
    dev->spi_rev = spi_rev;
    set_cs_high(dev->spi_fwd);
    set_cs_high(dev->spi_rev);

    dev->primary_dir    = ADBMS6380_DIR_FWD;
    dev->break_detected = false;
    dev->break_index    = ADBMS6380_NO_BREAK_INDEX;
    dev->reachable_fwd  = (uint8_t)ADBMS_CHIP_COUNT;
    dev->reachable_rev  = (uint8_t)ADBMS_CHIP_COUNT;

    dev->tx_strbuf = (strbuf_t){
        .data    = dev->tx_buf,
        .length  = 0U,
        .max_len = ADBMS6380_SPI_TX_BUFFER_SIZE,
    };
}

void ADBMS6380_toggle_primary_dir(adbms6380_t *dev) {
    dev->primary_dir = get_opposite_dir(dev->primary_dir);
}

void ADBMS6380_reset_break_state(adbms6380_t *dev) {
    dev->reachable_fwd = (uint8_t)ADBMS_CHIP_COUNT;
    dev->reachable_rev = (uint8_t)ADBMS_CHIP_COUNT;
}

void ADBMS6380_wake(adbms6380_t *dev) {
    if (!dev->break_detected) {
        wake_dir(dev, dev->primary_dir, ADBMS_CHIP_COUNT);
        return;
    }

    size_t fwd_pulses = dev->reachable_fwd;
    size_t rev_pulses = dev->reachable_rev;
    if (fwd_pulses + rev_pulses < ADBMS_CHIP_COUNT) {
        fwd_pulses = ADBMS_CHIP_COUNT;
        rev_pulses = ADBMS_CHIP_COUNT;
    }

    if (fwd_pulses > 0U) {
        wake_dir(dev, ADBMS6380_DIR_FWD, fwd_pulses);
    }
    if (rev_pulses > 0U) {
        if (fwd_pulses > 0U) {
            delay_tblock();
        }
        wake_dir(dev, ADBMS6380_DIR_REV, rev_pulses);
    }
}

void ADBMS6380_write_rega(adbms6380_t *dev, const adbms6380_rega_config_t *cfg) {
    pack_cfg_rega(dev->rega, cfg);
    if (!dev->break_detected) {
        write_rega_dir(dev, dev->primary_dir);
        return;
    }

    write_rega_dir(dev, ADBMS6380_DIR_FWD);
    delay_tblock();
    write_rega_dir(dev, ADBMS6380_DIR_REV);
}

bool ADBMS6380_verify_rega(adbms6380_t *dev) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        dev->chips[i].err_rega_mismatch = true;
    }

    bool all_reached = read_register_group(
        dev,
        ADBMS6380_CMD_RDCFGA,
        extract_rega_verify_slot,
        0U,
        write_rega_dir,
        true
    );
    dev->err_rega_pec = !all_reached;
    if (!all_reached) {
        return false;
    }

    return !ADBMS6380_has_rega_mismatch(dev);
}

void ADBMS6380_write_regb(adbms6380_t *dev, const adbms6380_regb_config_t *cfg) {
    for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
        pack_cfg_regb(dev->chips[chip_idx].regb, cfg, dev->chips[chip_idx].is_discharging);
    }

    if (!dev->break_detected) {
        write_regb_dir(dev, dev->primary_dir);
        return;
    }

    if (dev->reachable_fwd > 0U) {
        write_regb_dir(dev, ADBMS6380_DIR_FWD);
    }
    if (dev->reachable_rev > 0U) {
        if (dev->reachable_fwd > 0U) {
            delay_tblock();
        }
        write_regb_dir(dev, ADBMS6380_DIR_REV);
    }
}

bool ADBMS6380_verify_regb(adbms6380_t *dev) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        dev->chips[i].err_regb_mismatch = true;
    }

    bool all_reached = read_register_group(
        dev,
        ADBMS6380_CMD_RDCFGB,
        extract_regb_verify_slot,
        0U,
        write_regb_dir,
        true
    );
    dev->err_regb_pec = !all_reached;
    if (!all_reached) {
        return false;
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
    broadcast_cmd(dev);
}

void ADBMS6380_start_adax(adbms6380_t *dev, const adbms6380_adax_config_t *cfg) {
    strbuf_clear(&dev->tx_strbuf);
    uint8_t adax_cmd[ADBMS6380_COMMAND_RAW_SIZE] = {0};
    encode_adax(adax_cmd, cfg);
    prepare_command(&dev->tx_strbuf, adax_cmd);
    broadcast_cmd(dev);
}

void ADBMS6380_read_cell_voltages(adbms6380_t *dev) {
    for (size_t cmd_idx = 0U; cmd_idx < countof(RDCV_CMDS); cmd_idx++) {
        bool all_reached = read_register_group(
            dev,
            RDCV_CMDS[cmd_idx],
            extract_cell_voltage_slot,
            cmd_idx,
            nullptr,
            false
        );
        dev->err_cell_voltage_pecs[cmd_idx] = !all_reached;
    }
}

void ADBMS6380_read_gpio_voltages(adbms6380_t *dev) {
    for (size_t cmd_idx = 0U; cmd_idx < countof(RDAUX_CMDS); cmd_idx++) {
        bool all_reached = read_register_group(
            dev,
            RDAUX_CMDS[cmd_idx],
            extract_gpio_voltage_slot,
            cmd_idx,
            nullptr,
            false
        );
        dev->err_gpio_voltage_pecs[cmd_idx] = !all_reached;
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
    // Read the completed AUX results from the previous cycle's ADAX command,
    // then start the next ADAX conversion for the following cycle.
    ADBMS6380_read_gpio_voltages(dev);
    ADBMS6380_start_adax(dev, cfg);

    // Convert GPIO voltages to cell and onboard temperatures
    // GPIO voltage -> R2 in the thermistor divider -> temperature via respective thermistor curve
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS_CELL_THERMS_PER_CHIP; j++) {
            float r2                    = gpio_v_to_r2(dev->chips[i].gpio_voltages[j]);
            dev->chips[i].cell_temps[j] = cell_thermistor_R_to_T(r2);
        }
        float board_r2           = gpio_v_to_r2(dev->chips[i].gpio_voltages[ADBMS_CELL_THERMS_PER_CHIP]);
        dev->chips[i].board_temp = board_thermistor_R_to_T(board_r2);
    }
}
