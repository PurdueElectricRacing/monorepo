/**
 * @file soc_ukf.h
 * @brief Unscented Kalman filter for state of charge estimation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef SOC_UKF_H
#define SOC_UKF_H

#include "linear_algebra.h"

// $n = \dim(x) = 3$
constexpr int STATE_SIZE = 3;

// $2n + 1$
constexpr int NUM_SIGMA_POINTS = 2 * STATE_SIZE + 1;

typedef struct {
    float r0;   // ohms
    float r1;   // ohms
    float r2;   // ohms
    float tau1; // seconds
    float tau2; // seconds
    float Q;    // Ah
} ecm_params_t;

typedef struct {
    // mutating filter state
    vec3_t x; // [SoC, V1, V2]
    mat3_t P; // state covariance

    // const after init
    mat3_t A;
    vec3_t B;
    mat3_t Q; // process noise covariance
    float R;  // measurement noise covariance

    ecm_params_t ecm;
    float prediction_dt;

    float lambda;

    float mean_weights[NUM_SIGMA_POINTS];
    float covariance_weights[NUM_SIGMA_POINTS];
} soc_ukf_t;

typedef struct {
    ecm_params_t ecm;
    float prediction_dt;
    mat3_t Q;
    float R;
    float alpha;
    float beta;
    float kappa;
} soc_ukf_params_t;

// todo fit the real params
static constexpr soc_ukf_params_t LV_PARAMS = {
    .ecm = {
        .r0   = 0.015f,
        .r1   = 0.015f,
        .r2   = 0.015f,
        .tau1 = 100.0f,
        .tau2 = 1000.0f,
        .Q    = 5.0f,
    },
    .prediction_dt = 0.01f,
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

/**
 * @brief Initialize the SoC filter
 *
 * @param ukf Filter instance to initialize
 * @param params Cell model and filter parameters
 * @param x0 Initial state [SoC, V1, V2]
 * @param P0 Initial state covariance
 * ! P0 must be SPD
 */
void SOC_UKF_init(
    soc_ukf_t *ukf,
    const soc_ukf_params_t *params,
    vec3_t x0,
    mat3_t P0
);

/**
 * @brief Predict the next state and covariance
 *
 * @param ukf Filter instance
 * @param current Equivalent cell current in amps.
 * Positive current = discharge
 */
void SOC_UKF_predict(
    soc_ukf_t *ukf,
    float current
);

/**
 * @brief Correct the predicted state using measured voltage
 *
 * @param ukf Filter instance.
 * @param voltage Equivalent measured cell terminal voltage
 * @param current Cell current in amps
 * Positive current = discharge
 */
void SOC_UKF_update(
    soc_ukf_t *ukf,
    float voltage,
    float current
);

/**
 * @brief Get the current state of charge estimate
 *
 * @param ukf Filter instance.
 * @return Estimated SoC
 * SoC may be outside of [0.0, 1.0]
 */
static inline float GET_SOC(const soc_ukf_t *ukf) {
    return ukf->x.x;
}

#endif // SOC_UKF_H
