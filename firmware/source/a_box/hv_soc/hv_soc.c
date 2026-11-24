/**
 * @file hv_soc.c
 * @brief HV pack state of charge estimation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "hv_soc.h"

#include "common/linear_algebra/linear_algebra.h"
#include "common/soc_ukf/soc_ukf.h"
#include "main.h"
#include "telemetry.h"

soc_ukf_t soc_ukf = {0};

static constexpr soc_ukf_params_t HV_PARAMS = {
    .ecm = {
        .r0   = 0.0000100f,
        .r1   = 0.00804391f,
        .r2   = 0.00740416f,
        .tau1 = 0.140246f,
        .tau2 = 13.0688f,
        .Q    = 5.0f,
    },
    .prediction_dt = (float)HV_SOC_PERIOD_MS / 1000.0f,
    .Q = {{
        {4e-12f, 0.0f,  0.0f},
        {0.0f,   4e-8f, 0.0f},
        {0.0f,   0.0f,  4e-4f},
    }},
    .R     = 4e-4f,
    .alpha = 1.0f,
    .beta  = 2.0f,
    .kappa = 0.0f,
};

static float get_cell_current() {
    static constexpr float PACK_PARALLEL = 2.0f;

    uint16_t current_raw = adc1_dma_buffer.isense_raw;
    
    float pack_current = isense_to_current(current_raw);
    float cell_current = pack_current / (float)PACK_PARALLEL;

    return cell_current;
}

static float get_cell_voltage() {
    static constexpr float PACK_SERIES = 145.0f;

    // todo update off BMS
    uint16_t voltage_raw = adc1_dma_buffer.vbatt_raw;

    float pack_voltage = vbatt_to_voltage(voltage_raw);
    float cell_voltage = pack_voltage / (float)PACK_SERIES;

    return cell_voltage;
}

void hv_soc_init() {

    // todo inverse SOC lookup based on voltage at rest
    vec3_t x0 = {0.5f, 0.0f, 0.0f};
    mat3_t P0 = {{
        {2.5e-3f, 0.0f, 0.0f},
        {0.0f, 4e-4f, 0.0f},
        {0.0f, 0.0f, 4e-4f},
    }};

    SOC_UKF_init(
        &soc_ukf,
        &HV_PARAMS,
        x0,
        P0
    );
}

void hv_soc_periodic() {
    float cell_voltage = get_cell_voltage();
    float cell_current = get_cell_current();

    SOC_UKF_predict(
        &soc_ukf,
        cell_current
    );

    SOC_UKF_update(
        &soc_ukf,
        cell_voltage,
        cell_current
    );
}