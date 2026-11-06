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

typedef struct {
    float min;
    float max;
    float sum;
} adbms_stats_t;

static adbms_stats_t reduce_values(const float *values, size_t count) {
    adbms_stats_t stats = {
        .min = values[0],
        .max = values[0],
        .sum = 0.0f,
    };
    for (size_t i = 0; i < count; i++) {
        float val = values[i];
        if (val > stats.max) {
            stats.max = val;
        }
        if (val < stats.min) {
            stats.min = val;
        }
        stats.sum += val;
    }
    return stats;
}

static void merge_stats(adbms_stats_t *total, adbms_stats_t part) {
    if (part.max > total->max) {
        total->max = part.max;
    }
    if (part.min < total->min) {
        total->min = part.min;
    }
    total->sum += part.sum;
}

static void clear_discharging(adbms_bms_t *bms) {
    for (size_t chip_idx = 0U; chip_idx < ADBMS_CHIP_COUNT; chip_idx++) {
        for (size_t cell_idx = 0U; cell_idx < ADBMS6380_CELL_COUNT; cell_idx++) {
            bms->dev.chips[chip_idx].is_discharging[cell_idx] = false;
        }
    }
}

static void clear_unused_discharging(adbms_bms_t *bms) {
    for (size_t module_idx = 0U; module_idx < ADBMS_MODULE_COUNT; module_idx++) {
        size_t chip_a_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_A);
        for (size_t ch = ADBMS_CHIP_A_CELL_COUNT; ch < ADBMS6380_CELL_COUNT; ch++) {
            bms->dev.chips[chip_a_idx].is_discharging[ch] = false;
        }

        size_t chip_b_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_B);
        for (size_t ch = ADBMS_CHIP_B_CELL_COUNT; ch < ADBMS6380_CELL_COUNT; ch++) {
            bms->dev.chips[chip_b_idx].is_discharging[ch] = false;
        }
    }
}

static void write_regb(adbms_bms_t *bms) {
    clear_unused_discharging(bms);
    ADBMS6380_write_regb(&bms->dev, &ADBMS_REGB_CFG);
}

static void calculate_balance_cells(adbms_bms_t *bms, float min_voltage, float min_delta) {
    clear_discharging(bms);
    if (!bms->is_balancing_enabled || bms->min_voltage < min_voltage) {
        return;
    }

    float balance_threshold = bms->min_voltage + min_delta;
    for (size_t module_idx = 0U; module_idx < ADBMS_MODULE_COUNT; module_idx++) {
        for (size_t cell_idx = 0U; cell_idx < ADBMS_CELLS_PER_MODULE; cell_idx++) {
            adbms_channel_map_t map = ADBMS_map_cell(module_idx, cell_idx);
            bms->dev.chips[map.chip_idx].is_discharging[map.channel_idx] =
                bms->dev.chips[map.chip_idx].cell_voltages[map.channel_idx] > balance_threshold;
        }
    }
}

static void read_cells(adbms_bms_t *bms) {
    ADBMS6380_read_cell_voltages(&bms->dev);

    // Calculate min/avg/max for overall pack using per-module reductions of connected cells
    float first_cell_v = ADBMS_get_cell_voltage(bms, 0U, 0U);
    adbms_stats_t pack_stats = {
        .min = first_cell_v,
        .max = first_cell_v,
        .sum = 0.0f,
    };

    for (size_t module_idx = 0U; module_idx < ADBMS_MODULE_COUNT; module_idx++) {
        size_t chip_a_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_A);
        size_t chip_b_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_B);

        adbms_stats_t chip_a_stats =
            reduce_values(bms->dev.chips[chip_a_idx].cell_voltages, ADBMS_CHIP_A_CELL_COUNT);
        adbms_stats_t chip_b_stats =
            reduce_values(bms->dev.chips[chip_b_idx].cell_voltages, ADBMS_CHIP_B_CELL_COUNT);

        merge_stats(&pack_stats, chip_a_stats);
        merge_stats(&pack_stats, chip_b_stats);
    }

    bms->min_voltage = pack_stats.min;
    bms->max_voltage = pack_stats.max;
    bms->sum_voltage = pack_stats.sum;
    bms->avg_voltage = pack_stats.sum / (float)ADBMS_PACK_CELL_COUNT;
}

static void read_therms(adbms_bms_t *bms) {
    ADBMS6380_read_temperatures(&bms->dev, &ADBMS_ADAX_CFG);

    // Calculate min/avg/max thermistor temps for overall pack across all populated thermistors
    float first_therm_temp = ADBMS_get_therm_temp(bms, 0U, 0U);
    adbms_stats_t pack_stats = {
        .min = first_therm_temp,
        .max = first_therm_temp,
        .sum = 0.0f,
    };

    for (size_t module_idx = 0U; module_idx < ADBMS_MODULE_COUNT; module_idx++) {
        size_t chip_a_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_A);
        size_t chip_b_idx = ADBMS_module_chip_index(module_idx, ADBMS_CHIP_ROLE_B);

        adbms_stats_t chip_a_stats =
            reduce_values(bms->dev.chips[chip_a_idx].therms_temps, ADBMS_THERMISTORS_PER_CHIP);
        adbms_stats_t chip_b_stats =
            reduce_values(bms->dev.chips[chip_b_idx].therms_temps, ADBMS_THERMISTORS_PER_CHIP);

        merge_stats(&pack_stats, chip_a_stats);
        merge_stats(&pack_stats, chip_b_stats);
    }

    bms->min_therm_temp = pack_stats.min;
    bms->max_therm_temp = pack_stats.max;
    bms->avg_therm_temp = pack_stats.sum / (float)ADBMS_PACK_THERMISTOR_COUNT;
}

// Catch corrupted readings
static bool is_valid_voltage(float voltage) {
    return voltage >= ADBMS_CELL_MIN_VALID_V && voltage <= ADBMS_CELL_MAX_VALID_V;
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

            write_regb(bms);
            if (!ADBMS6380_verify_regb(&bms->dev)) {
                next_state = ADBMS_STATE_RECOVERING;
                break;
            }

            bms->recovery_attempts = 0U; // successful connect
            ADBMS6380_start_adcv(&bms->dev, &ADBMS_ADCV_CFG);
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

            calculate_balance_cells(bms, min_voltage_for_balance, min_delta_for_balance);

            // apply new discharge bits
            write_regb(bms);
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
