/**
 * @file adbms.c
 * @brief Primary logic and interface for ADBMS battery management system driver.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "adbms.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "adbms6380.h"
#include "commands.h"
#include "common/phal_G4/spi/spi.h"
#include "common/strbuf/strbuf.h"
#include "thermistor.h"
#include "common/rtos/rtos.h"

void adbms_init(adbms_bms_t *bms, SPI_InitConfig_t *spi, uint8_t *tx_buf) {
    bms->spi   = spi;
    bms->state = ADBMS_STATE_IDLE;

    bms->is_balancing_enabled = false;
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0; j < ADBMS6380_CELL_COUNT; j++) {
            bms->modules[i].is_discharging[j] = false;
        }
        bms->modules[i].err_rega_mismatch = false;
        bms->modules[i].err_regb_mismatch = false;
    }

    bms->err_spi           = false;
    bms->err_connect       = false;
    bms->err_rega_mismatch = false;
    bms->err_regb_mismatch = false;
    bms->err_rega_pec      = false;
    bms->err_regb_pec      = false;

    for (size_t i = 0; i < ADBMS6380_RDCV_CMD_COUNT; i++) {
        bms->err_cell_voltage_pecs[i] = false;
    }
    for (size_t i = 0; i < ADBMS6380_RDAUX_CMD_COUNT; i++) {
        bms->err_gpio_voltage_pecs[i] = false;
    }

    bms->tx_strbuf = (strbuf_t) {
        .data    = tx_buf,
        .length  = 0,
        .max_len = ADBMS_SPI_TX_BUFFER_SIZE,
    };
}

void adbms_write_rega(adbms_bms_t *bms) {
    strbuf_clear(&bms->tx_strbuf);
    adbms6380_prepare_command(&bms->tx_strbuf, WRCFGA);
    // i must be signed
    for (int i = ADBMS_MODULE_COUNT - 1; i >= 0; i--) {
        adbms6380_calculate_cfg_rega(bms->modules[i].rega, ADBMS_REGA_REFON, ADBMS_REGA_CTH);
        adbms6380_prepare_data_packet(&bms->tx_strbuf, bms->modules[i].rega);
    }

    adbms6380_set_cs_low(bms->spi);
    PHAL_SPI_transferBlocking(bms->spi, bms->tx_strbuf.data, NULL, bms->tx_strbuf.length);
    adbms6380_set_cs_high(bms->spi);

}

void adbms_write_regb(adbms_bms_t *bms) {
    strbuf_clear(&bms->tx_strbuf);
    adbms6380_prepare_command(&bms->tx_strbuf, WRCFGB);
    // i must be signed
    for (int i = ADBMS_MODULE_COUNT - 1; i >= 0; i--) {
        adbms6380_calculate_cfg_regb(bms->modules[i].regb,
                                     ADBMS_REGB_OV_THRESHOLD,
                                     ADBMS_REGB_UV_THRESHOLD,
                                     bms->modules[i].is_discharging);
        adbms6380_prepare_data_packet(&bms->tx_strbuf, bms->modules[i].regb);
    }

    adbms6380_set_cs_low(bms->spi);
    PHAL_SPI_transferBlocking(bms->spi, bms->tx_strbuf.data, NULL, bms->tx_strbuf.length);
    adbms6380_set_cs_high(bms->spi);
}

bool adbms_read_and_check_rega(adbms_bms_t *bms) {
    strbuf_clear(&bms->tx_strbuf);
    adbms6380_prepare_command(&bms->tx_strbuf, RDCFGA);
    adbms6380_read_result_t rdcfga_result =
        adbms6380_read_data_with_retries(bms->spi,
                                         ADBMS_PEC_FAIL_MAX_RETRIES,
                                         ADBMS_MODULE_COUNT,
                                         bms->tx_strbuf.data,
                                         bms->rx_buf);
    switch (rdcfga_result) {
        case ADBMS6380_READ_SUCCESS:
            // continue to check for mismatch
            bms->err_rega_pec = false;
            break;
        case ADBMS6380_READ_PEC_FAILURE:
            bms->err_rega_pec = true;
            return false;
        case ADBMS6380_READ_SPI_FAILURE:
            bms->err_spi = true;
            return false;
    }
    bms->err_rega_mismatch = false;
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        const uint8_t *module_data = &bms->rx_buf[i * ADBMS6380_SINGLE_DATA_PKT_SIZE];
        if (memcmp(&module_data[0], bms->modules[i].rega, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0) {
            bms->err_rega_mismatch            = true;
            bms->modules[i].err_rega_mismatch = true;
        } else {
            bms->modules[i].err_rega_mismatch = false;
        }
    }
    return !bms->err_rega_mismatch;
}

bool adbms_read_and_check_regb(adbms_bms_t *bms) {
    strbuf_clear(&bms->tx_strbuf);
    adbms6380_prepare_command(&bms->tx_strbuf, RDCFGB);
    adbms6380_read_result_t rdcfgb_result =
        adbms6380_read_data_with_retries(bms->spi,
                                         ADBMS_PEC_FAIL_MAX_RETRIES,
                                         ADBMS_MODULE_COUNT,
                                         bms->tx_strbuf.data,
                                         bms->rx_buf);
    switch (rdcfgb_result) {
        case ADBMS6380_READ_SUCCESS:
            // continue to check for mismatch
            bms->err_regb_pec = false;
            break;
        case ADBMS6380_READ_PEC_FAILURE:
            bms->err_regb_pec = true;
            return false;
        case ADBMS6380_READ_SPI_FAILURE:
            bms->err_spi = true;
            return false;
    }
    bms->err_regb_mismatch = false;
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        const uint8_t *module_data = &bms->rx_buf[i * ADBMS6380_SINGLE_DATA_PKT_SIZE];
        if (memcmp(&module_data[0], bms->modules[i].regb, ADBMS6380_SINGLE_DATA_RAW_SIZE) != 0) {
            bms->err_regb_mismatch            = true;
            bms->modules[i].err_regb_mismatch = true;
        } else {
            bms->modules[i].err_regb_mismatch = false;
        }
    }
    return !bms->err_regb_mismatch;
}

void adbms_connect(adbms_bms_t *bms) {
    adbms_write_rega(bms);
    adbms_write_regb(bms);
    if (!adbms_read_and_check_rega(bms)) {
        bms->state       = ADBMS_STATE_IDLE;
        bms->err_connect = true;
        return;
    }
    if (!adbms_read_and_check_regb(bms)) {
        bms->state       = ADBMS_STATE_IDLE;
        bms->err_connect = true;
        return;
    }

    // Start ADCV
    strbuf_clear(&bms->tx_strbuf);
    uint8_t adcv_cmd[2] = {0};
    adbms6380_adcv(adcv_cmd,
                   ADBMS_ADCV_RD,
                   ADBMS_ADCV_CONT,
                   ADBMS_ADCV_DCP,
                   ADBMS_ADCV_RSTF,
                   ADBMS_ADCV_OW);
    adbms6380_prepare_command(&bms->tx_strbuf, adcv_cmd);
    adbms6380_set_cs_low(bms->spi);
    PHAL_SPI_transferBlocking(bms->spi, bms->tx_strbuf.data, NULL, bms->tx_strbuf.length);
    adbms6380_set_cs_high(bms->spi);

    bms->err_connect = false;
    bms->state       = ADBMS_STATE_CONNECTED;
}

void adbms_calculate_balance_cells(adbms_bms_t *bms, float min_voltage, float min_delta) {
    if (!bms->is_balancing_enabled) {
        // Disable all discharging
        for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
            for (size_t j = 0; j < ADBMS6380_CELL_COUNT; j++) {
                bms->modules[i].is_discharging[j] = false;
            }
        }
        return;
    }

    if (bms->min_voltage < min_voltage) {
        for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
            for (size_t j = 0; j < ADBMS6380_CELL_COUNT; j++) {
                bms->modules[i].is_discharging[j] = false;
            }
        }

        return;
    } else {
        float balance_threshold = bms->min_voltage + min_delta;
        for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
            for (size_t j = 0; j < ADBMS6380_CELL_COUNT; j++) {
                if (bms->modules[i].cell_voltages[j] > balance_threshold) {
                    bms->modules[i].is_discharging[j] = true;
                } else {
                    bms->modules[i].is_discharging[j] = false;
                }
            }
        }
    }
}

void adbms_balance_and_update_regb(adbms_bms_t *bms, float min_voltage, float min_delta) {
    adbms_calculate_balance_cells(bms, min_voltage, min_delta);

    adbms_write_regb(bms);
    if (!adbms_read_and_check_regb(bms)) {
        bms->state = ADBMS_STATE_IDLE;
        return;
    }
}

void adbms_read_cells(adbms_bms_t *bms) {
    float *cell_voltage_ptrs[ADBMS_MODULE_COUNT]        = {0};
    int16_t *cell_voltages_raw_ptrs[ADBMS_MODULE_COUNT] = {0};
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        cell_voltage_ptrs[i]      = bms->modules[i].cell_voltages;
        cell_voltages_raw_ptrs[i] = bms->modules[i].cell_voltages_raw;
    }

    if (!adbms6380_read_cell_voltages(bms->spi,
                                      &bms->tx_strbuf,
                                      bms->rx_buf,
                                      cell_voltage_ptrs,
                                      cell_voltages_raw_ptrs,
                                      bms->err_cell_voltage_pecs,
                                      ADBMS_MODULE_COUNT,
                                      ADBMS_PEC_FAIL_MAX_RETRIES)) {
        bms->state   = ADBMS_STATE_IDLE;
        bms->err_spi = true;
        return;
    }

    // Calculate min/avg/max for each module and overall pack
    bms->max_voltage = bms->modules[0].cell_voltages[0];
    bms->min_voltage = bms->modules[0].cell_voltages[0];
    bms->sum_voltage = 0.0f;
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        bms->modules[i].max_voltage = bms->modules[i].cell_voltages[0];
        bms->modules[i].min_voltage = bms->modules[i].cell_voltages[0];
        bms->modules[i].sum_voltage = 0.0f;
        for (size_t j = 0; j < ADBMS6380_CELL_COUNT; j++) {
            float cell_v = cell_voltage_ptrs[i][j];
            if (cell_v > bms->modules[i].max_voltage) {
                bms->modules[i].max_voltage = cell_v;
            }
            if (cell_v < bms->modules[i].min_voltage) {
                bms->modules[i].min_voltage = cell_v;
            }
            bms->modules[i].sum_voltage += cell_v;
        }
        bms->modules[i].avg_voltage = bms->modules[i].sum_voltage / ADBMS6380_CELL_COUNT;

        if (bms->modules[i].max_voltage > bms->max_voltage) {
            bms->max_voltage = bms->modules[i].max_voltage;
        }
        if (bms->modules[i].min_voltage < bms->min_voltage) {
            bms->min_voltage = bms->modules[i].min_voltage;
        }
        bms->sum_voltage += bms->modules[i].sum_voltage;
    }
    bms->avg_voltage = bms->sum_voltage / (ADBMS_MODULE_COUNT * ADBMS6380_CELL_COUNT);
}

void adbms_read_therms(adbms_bms_t *bms) {
    float *gpio_voltage_ptrs[ADBMS_MODULE_COUNT] = {0};
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        gpio_voltage_ptrs[i] = bms->modules[i].therms_voltages;
    }

    // Start ADAX
    strbuf_clear(&bms->tx_strbuf);
    uint8_t adax_cmd[2] = {0};
    adbms6380_adax(adax_cmd, ADBMS_ADAX_OW, ADBMS_ADAX_PUP, ADBMS_ADAX_CH);
    adbms6380_prepare_command(&bms->tx_strbuf, adax_cmd);
    adbms6380_set_cs_low(bms->spi);
    PHAL_SPI_transferBlocking(bms->spi, bms->tx_strbuf.data, NULL, bms->tx_strbuf.length);
    adbms6380_set_cs_high(bms->spi);

    if (!adbms6380_read_gpio_voltages(bms->spi,
                                      &bms->tx_strbuf,
                                      bms->rx_buf,
                                      gpio_voltage_ptrs,
                                      bms->err_gpio_voltage_pecs,
                                      ADBMS_MODULE_COUNT,
                                      ADBMS_PEC_FAIL_MAX_RETRIES)) {
        bms->state   = ADBMS_STATE_IDLE;
        bms->err_spi = true;
        return;
    }

    // Convert all GPIO voltages to temperatures
    // GPIO voltage -> R2 in the thermistor divider -> temperature via thermistor curve
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0; j < ADBMS6380_GPIO_COUNT; j++) {
            float v_out = bms->modules[i].therms_voltages[j];
            // Avoid divide by 0:
            if (v_out == ADBMS_GPIO_VIN) {
                v_out = ADBMS_GPIO_VIN - 0.00001f;
            }
            float r2                        = (v_out * ADBMS_GPIO_R1) / (ADBMS_GPIO_VIN - v_out);
            bms->modules[i].therms_temps[j] = thermistor_R_to_T(r2);
        }
    }

    // Calculate min/avg/max thermistor temps for each module and overall
    bms->max_therm_temp      = bms->modules[0].therms_temps[0];
    bms->min_therm_temp      = bms->modules[0].therms_temps[0];
    float bms_sum_therm_temp = 0.0f;
    for (size_t i = 0; i < ADBMS_MODULE_COUNT; i++) {
        bms->modules[i].max_therm_temp = bms->modules[i].therms_temps[0];
        bms->modules[i].min_therm_temp = bms->modules[i].therms_temps[0];
        float module_sum_therm_temp    = 0.0f;
        for (size_t j = 0; j < ADBMS6380_GPIO_COUNT; j++) {
            float t = bms->modules[i].therms_temps[j];
            if (t > bms->modules[i].max_therm_temp) {
                bms->modules[i].max_therm_temp = t;
            }
            if (t < bms->modules[i].min_therm_temp) {
                bms->modules[i].min_therm_temp = t;
            }
            module_sum_therm_temp += t;
        }
        bms->modules[i].avg_therm_temp = module_sum_therm_temp / ADBMS6380_GPIO_COUNT;
        if (bms->modules[i].max_therm_temp > bms->max_therm_temp) {
            bms->max_therm_temp = bms->modules[i].max_therm_temp;
        }
        if (bms->modules[i].min_therm_temp < bms->min_therm_temp) {
            bms->min_therm_temp = bms->modules[i].min_therm_temp;
        }
        bms_sum_therm_temp += module_sum_therm_temp;
    }
    bms->avg_therm_temp = bms_sum_therm_temp / (ADBMS_MODULE_COUNT * ADBMS6380_GPIO_COUNT);
}

void adbms_periodic(adbms_bms_t *bms, float min_voltage_for_balance, float min_delta_for_balance) {
    static int connect_attempts = 0;
    adbms6380_wake(bms->spi, ADBMS_MODULE_COUNT);
    switch (bms->state) {
        case ADBMS_STATE_IDLE: {
            adbms_connect(bms);
            if (bms->state == ADBMS_STATE_CONNECTED) {
                connect_attempts = 0;
            } else {
                connect_attempts++;
                if (connect_attempts >= 4) {
                    bms->state = ADBMS_STATE_RECOVERING;
                }
            }
            break;
        }
        case ADBMS_STATE_CONNECTED: {
            adbms_read_cells(bms);
            adbms_read_therms(bms);
            adbms_balance_and_update_regb(bms, min_voltage_for_balance, min_delta_for_balance);

            // todo add more robust corruption checks
            bool corruption_detected = (bms->min_voltage < 1.0f) || (bms->max_voltage > 5.0f);
            if (corruption_detected) {
                bms->state = ADBMS_STATE_RECOVERING;
            }
            break;
        }
        case ADBMS_STATE_RECOVERING: {
            // Do a full sleep-wake cycle
            RTOS_delay_ms(2500); // ADBMS6380 chips sleep after 2.2s of inactivity
            connect_attempts = 0;
            bms->state = ADBMS_STATE_IDLE;
            break;
        }
    }
}
