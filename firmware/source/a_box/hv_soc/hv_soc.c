

#include "common/rtos/rtos.h"
#include "common/soc_ukf/soc_ukf.h"

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
    .prediction_dt = 0.01f,
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

float pack_to_cell_current(float pack_current) {
    static constexpr float PACK_PARALLEL = 3.0f;

    return pack_current / (float)PACK_PARALLEL;
}

float pack_to_cell_voltage(float pack_voltage) {
    static constexpr float PACK_SERIES = 128.0f;

    return pack_voltage / (float)PACK_SERIES;
}

void hv_soc_periodic() {
    uint16_t current_raw = adc1_dma_buffer.isense_raw;
    uint16_t voltage_raw = adc1_dma_buffer.vbatt_raw; // todo use BMS
    
    float pack_current = isense_to_current(current_raw);
    float pack_voltage = vbatt_to_voltage(voltage_raw);

    float cell_current = pack_to_cell_current(pack_current);
    float cell_voltage = pack_to_cell_voltage(pack_voltage);

    SOC_UFK_predict(
        &soc_ukf,
        cell_current
    );

    SOC_UFK_update(
        &soc_ukf,
        cell_voltage,
        cell_current
    );
}