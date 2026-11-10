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

#include "adbms6380.h"
#include "common/phal_G4/spi/spi.h"
#include "common/rtos/rtos.h"

// Wait longer than the ADBMS6380 ~2.2 s inactivity sleep timeout so that a
// full sleep-wake cycle elapses during recovery before we retry connecting
static constexpr uint32_t ADBMS_CHIP_SLEEP_CYCLE_MS = 2'500U;
static_assert(ADBMS_CHIP_SLEEP_CYCLE_MS > 2'200U, "cycle time must be longer than the chip sleep timeout");

// Number of consecutive failed recovery cycles (each a full sleep-wake
// attempt) before the driver enters the latching FATAL state
static constexpr uint8_t ADBMS_RECOVERY_MAX_ATTEMPTS = 4U;

// Readings outside these are treated as corrupted data rather than real measurements
// and causes a transition to RECOVERING.
static constexpr float ADBMS_CELL_MIN_VALID_V = 1.0f;
static constexpr float ADBMS_CELL_MAX_VALID_V = 5.0f;
static constexpr float ADBMS_TEMP_MIN_VALID_C = -20.0f;
static constexpr float ADBMS_TEMP_MAX_VALID_C = 100.0f;

static constexpr adbms6380_rega_config_t ADBMS_REGA_CFG = {
    .cth   = 0b110U, // 25.05 mV
    .refon = true,
};

static constexpr adbms6380_regb_config_t ADBMS_REGB_CFG = {
    .overvoltage_threshold_v  = 4.2f,
    .undervoltage_threshold_v = 3.0f,
};

static constexpr adbms6380_adcv_config_t ADBMS_ADCV_CFG = {
    .ow   = 0b00U,
    .rd   = false,
    .cont = true, // Continuous
    .dcp  = false,
    .rstf = true,
};

static constexpr adbms6380_adax_config_t ADBMS_ADAX_CFG = {
    .ch  = 0b00000U,
    .ow  = false,
    .pup = false,
};

static void clear_discharging(adbms_bms_t *bms) {
    for (size_t i = 0U; i < ADBMS_CHIP_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS6380_CELL_COUNT; j++) {
            bms->dev.chips[i].is_discharging[j] = false;
        }
    }
}

static void calculate_balance_cells(adbms_bms_t *bms, float min_voltage, float min_delta) {
    if (!bms->is_balancing_enabled || bms->min_voltage < min_voltage) {
        clear_discharging(bms);
        return;
    }

    float balance_threshold = bms->min_voltage + min_delta;
    for (size_t i = 0U; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS_CELLS_PER_MODULE; j++) {
            size_t chip_idx = (i * ADBMS_CHIPS_PER_MODULE) + (j / ADBMS6380_CELL_COUNT);
            size_t chan_idx = j % ADBMS6380_CELL_COUNT;
            bms->dev.chips[chip_idx].is_discharging[chan_idx] =
                bms->dev.chips[chip_idx].cell_voltages[chan_idx] > balance_threshold;
        }
    }
}

static void read_cells(adbms_bms_t *bms) {
    ADBMS6380_read_cell_voltages(&bms->dev);

    // Calculate min/avg/max for overall pack
    bms->max_voltage = ADBMS_get_cell_voltage(bms, 0U, 0U);
    bms->min_voltage = ADBMS_get_cell_voltage(bms, 0U, 0U);
    bms->sum_voltage = 0.0f;
    for (size_t i = 0U; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS_CELLS_PER_MODULE; j++) {
            float val = ADBMS_get_cell_voltage(bms, i, j);
            if (val > bms->max_voltage) {
                bms->max_voltage = val;
            }
            if (val < bms->min_voltage) {
                bms->min_voltage = val;
            }
            bms->sum_voltage += val;
        }
    }
    bms->avg_voltage = bms->sum_voltage / (float)(ADBMS_MODULE_COUNT * ADBMS_CELLS_PER_MODULE);
}

static void read_therms(adbms_bms_t *bms) {
    ADBMS6380_read_temperatures(&bms->dev, &ADBMS_ADAX_CFG);

    // Calculate min/avg/max cell thermistor temps for overall pack
    bms->max_cell_temp      = ADBMS_get_cell_temp(bms, 0U, 0U);
    bms->min_cell_temp      = ADBMS_get_cell_temp(bms, 0U, 0U);
    float bms_sum_cell_temp = 0.0f;
    for (size_t i = 0U; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS_CELL_THERMS_PER_MODULE; j++) {
            float val = ADBMS_get_cell_temp(bms, i, j);
            if (val > bms->max_cell_temp) {
                bms->max_cell_temp = val;
            }
            if (val < bms->min_cell_temp) {
                bms->min_cell_temp = val;
            }
            bms_sum_cell_temp += val;
        }
    }
    size_t total_cell_therms = ADBMS_MODULE_COUNT * ADBMS_CELL_THERMS_PER_MODULE;
    bms->avg_cell_temp = bms_sum_cell_temp / (float)(total_cell_therms);

    // Calculate min/avg/max onboard PCB thermistor temps across all chips
    bms->max_board_temp      = ADBMS_get_board_temp(bms, 0U, 0U);
    bms->min_board_temp      = ADBMS_get_board_temp(bms, 0U, 0U);
    float bms_sum_board_temp = 0.0f;
    for (size_t i = 0U; i < ADBMS_MODULE_COUNT; i++) {
        for (size_t j = 0U; j < ADBMS_BOARD_THERMS_PER_MODULE; j++) {
            float val = ADBMS_get_board_temp(bms, i, j);
            if (val > bms->max_board_temp) {
                bms->max_board_temp = val;
            }
            if (val < bms->min_board_temp) {
                bms->min_board_temp = val;
            }
            bms_sum_board_temp += val;
        }
    }
    size_t total_board_therms = ADBMS_MODULE_COUNT * ADBMS_BOARD_THERMS_PER_MODULE;
    bms->avg_board_temp = bms_sum_board_temp / (float)(total_board_therms);
}

// Catch corrupted readings
static bool is_valid_voltage(float voltage) {
    return voltage >= ADBMS_CELL_MIN_VALID_V && voltage <= ADBMS_CELL_MAX_VALID_V;
}

static bool is_valid_temp(float temp) {
    return temp >= ADBMS_TEMP_MIN_VALID_C && temp <= ADBMS_TEMP_MAX_VALID_C;
}

void ADBMS_init(adbms_bms_t *bms, SPI_InitConfig_t *spi) {
    *bms = (adbms_bms_t){0};
    ADBMS6380_init(&bms->dev, spi);

    bms->recover_start_tick_ms = 0U;
    bms->recovery_attempts     = 0U;
    bms->state                 = ADBMS_STATE_CONNECTING;
    bms->is_balancing_enabled  = false;
}

void ADBMS_periodic(adbms_bms_t *bms, float min_voltage_for_balance, float min_delta_for_balance) {
    adbms_state_t next_state = bms->state;

    // Keep recovery/fatal silent to complete sleep cycle
    // Otherwise prep the chips for communication
    if (bms->state != ADBMS_STATE_RECOVERING && bms->state != ADBMS_STATE_FATAL) {
        ADBMS6380_wake(&bms->dev);
    }

    switch (bms->state) {
        case ADBMS_STATE_CONNECTING: {
            // Run the connect sequence
            // Any verification failure goes to RECOVERING.
            ADBMS6380_write_rega(&bms->dev, &ADBMS_REGA_CFG);
            if (!ADBMS6380_verify_rega(&bms->dev)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }

            ADBMS6380_write_regb(&bms->dev, &ADBMS_REGB_CFG);
            if (!ADBMS6380_verify_regb(&bms->dev)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }

            bms->recovery_attempts = 0U; // successful connect
            ADBMS6380_start_adcv(&bms->dev, &ADBMS_ADCV_CFG);
            ADBMS6380_start_adax(&bms->dev, &ADBMS_ADAX_CFG);
            next_state = ADBMS_STATE_READ_LOOP;
            break;
        }
        case ADBMS_STATE_READ_LOOP: {
            // One full read/balance cycle
            // Any failure goes to RECOVERING
            read_cells(bms);
            if (!is_valid_voltage(bms->min_voltage) || !is_valid_voltage(bms->max_voltage)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }
            read_therms(bms);
            if (!is_valid_temp(bms->min_cell_temp) || !is_valid_temp(bms->max_cell_temp)
                || !is_valid_temp(bms->min_board_temp) || !is_valid_temp(bms->max_board_temp)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }

            calculate_balance_cells(bms, min_voltage_for_balance, min_delta_for_balance);

            // apply new discharge bits
            ADBMS6380_write_regb(&bms->dev, &ADBMS_REGB_CFG);
            if (!ADBMS6380_verify_regb(&bms->dev)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }

            next_state = ADBMS_STATE_READ_LOOP;
            break;
        }
        case ADBMS_STATE_RECOVERING: {
            // Either redirect to FATAL or
            // non-blocking wait for a full chip sleep-wake cycle with backoff

            // Already attempted max recovery attempts, go to fatal
            if (bms->recovery_attempts >= ADBMS_RECOVERY_MAX_ATTEMPTS) {
                next_state = ADBMS_STATE_FATAL;
                break;
            }

            // Else, sleep-wake cycle with backoff
            uint32_t elapsed_ms       = xTaskGetTickCount() - bms->recover_start_tick_ms;
            uint32_t total_sleep_time = ADBMS_CHIP_SLEEP_CYCLE_MS * (bms->recovery_attempts + 1U);
            if (elapsed_ms >= total_sleep_time) {
                bms->recovery_attempts++;
                next_state = ADBMS_STATE_CONNECTING;
            }

            break;
        }
        case ADBMS_STATE_FATAL: {
            // Latching terminal state after repeated failed recovery
            break;
        }
    }

    // Capture state entry timestamps
    if (next_state == ADBMS_STATE_RECOVERING && bms->state != ADBMS_STATE_RECOVERING) {
        bms->recover_start_tick_ms = xTaskGetTickCount();
    }

    // Update the state
    bms->state = next_state;
}
