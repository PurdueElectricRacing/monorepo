#ifndef SOC_UKF_H
#define SOC_UKF_H

#include "../linear_algebra/linear_algebra.h"

constexpr int STATE_SIZE = 3;
constexpr int NUM_SIGMA_POINTS = 2 * STATE_SIZE + 1; // 2n + 1

typedef struct {
    float r0;   // ohms
    float r1;   // ohms
    float r2;   // ohms
    float tau1; // seconds
    float tau2; // seconds
    float Q;    // Ah
} cell_params_t;

typedef struct {
    cell_params_t cell;
    float dt;
    mat3_t Q;
    float R;
    float alpha;
    float beta;
    float kappa;
} soc_ukf_params_t;

typedef struct {
    // mutating filter state
    vec3_t x; // [SoC, V1, V2]
    mat3_t P; // state covariance

    // constants
    mat3_t A; // state transition
    vec3_t B; // control matrix
    mat3_t Q; // process noise covariance
    float R;  // measurement noise covariance

    cell_params_t cell;
    float dt;

    float lambda;

    float mean_weights[NUM_SIGMA_POINTS];
    float covariance_weights[NUM_SIGMA_POINTS];
} soc_ukf_t;


// todo fir the real params
static constexpr soc_ukf_params_t LV_PARAMS = {
    .cell = {
        .r0   = 0.015f,
        .r1   = 0.015f,
        .r2   = 0.015f,
        .tau1 = 100.0f,
        .tau2 = 1000.0f,
        .Q    = 5.0f,
    },
    .dt = 0.01f,
    .Q  = {{
        {1e-6f, 0.0f,  0.0f},
        {0.0f,  1e-6f, 0.0f},
        {0.0f,  0.0f,  1e-6f},
    }},
    .R     = 1e-3f,
    .alpha = 1.0f,
    .beta  = 2.0f,
    .kappa = 0.0f,
};

void SOC_UKF_init(
    soc_ukf_t *ukf,
    const soc_ukf_params_t *params,
    vec3_t x0,
    mat3_t P0
);

void SOC_UKF_predict(soc_ukf_t *ukf, float current);
void SOC_UKF_update(soc_ukf_t *ukf, float voltage);

#endif // SOC_UKF_H
