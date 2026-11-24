/**
 * @file hv_soc.c
 * @brief HV pack state of charge estimation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "hv_soc.h"

#include "lerp_lut/lerp_lut.h"
#include "linear_algebra/linear_algebra.h"
#include "soc_ukf/soc_ukf.h"
#include "main.h"
#include "telemetry.h"

soc_ukf_t soc_ukf = {0};

// todo a shared home with PDU
static constexpr size_t OCV_TO_SOC_SIZE = 11;
static const lut_entry_t OCV_TO_SOC_ENTRIES[OCV_TO_SOC_SIZE] = {
    {2.811f, 0.0f},
    {3.096f, 0.1f},
    {3.286f, 0.2f},
    {3.439f, 0.3f},
    {3.570f, 0.4f},
    {3.690f, 0.5f},
    {3.829f, 0.6f},
    {3.906f, 0.7f},
    {4.040f, 0.8f},
    {4.085f, 0.9f},
    {4.171f, 1.0f},
};
LERP_LUT_INIT(OCV_TO_SOC, OCV_TO_SOC_ENTRIES, OCV_TO_SOC_SIZE);

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
    float cell_voltage = get_cell_voltage();
    float initial_soc = lut_lookup(&OCV_TO_SOC, cell_voltage);

    vec3_t x0 = {
        initial_soc,
        0.0f,
        0.0f
    };

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