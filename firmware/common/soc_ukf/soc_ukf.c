/**
 * @file soc_ukf.c
 * @brief Unscented Kalman filter for state of charge estimation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "soc_ukf.h"

#include <math.h>

#include "lerp_lut.h"

// at 35C
// todo add 2d lookup + temperature ?
static constexpr size_t OCV_LUT_SIZE = 11;
static const lut_entry_t OCV_ENTRIES[OCV_LUT_SIZE] = {
    {0.0f, 2.811f},
    {0.1f, 3.096f},
    {0.2f, 3.286f},
    {0.3f, 3.439f},
    {0.4f, 3.570f},
    {0.5f, 3.690f},
    {0.6f, 3.829f},
    {0.7f, 3.906f},
    {0.8f, 4.040f},
    {0.9f, 4.085f},
    {1.0f, 4.171f}
};

LERP_LUT_INIT(OCV_LUT, OCV_ENTRIES, OCV_LUT_SIZE);

static void SOC_UKF_mean_weights(
    const soc_ukf_t *ukf,
    float mean_weights[NUM_SIGMA_POINTS]
) {
    float scale = STATE_SIZE + ukf->lambda;
    
    /*
    $$
    W_0^{(m)} = \frac{\lambda}{n+\lambda}
    $$
    */
    mean_weights[0] = ukf->lambda / scale;

    /*
    $$
    W_i^{(m)} = \frac{1}{2(n+\lambda)}, \qquad i=1,\ldots,2n
    $$
    */
    for (int i = 1; i < NUM_SIGMA_POINTS; i++) {
        mean_weights[i] = 1.0f / (2.0f * scale);
    }
}

static void SOC_UKF_covariance_weights(
    soc_ukf_t *ukf,
    const soc_ukf_params_t *params
) {
    /*
    $$
    W_0^{(c)} = W_0^{(m)} + 1 - \alpha^2 + \beta
    $$
    */
    ukf->covariance_weights[0] =
        ukf->mean_weights[0]
        + 1.0f
        - params->alpha * params->alpha
        + params->beta;

    /*
    $$
    W_i^{(c)} = W_i^{(m)} \quad (i > 0)
    $$
    */
    for (int i = 1; i < NUM_SIGMA_POINTS; i++) {
        ukf->covariance_weights[i] = ukf->mean_weights[i];
    }
}

void SOC_UKF_init(
    soc_ukf_t *ukf,
    const soc_ukf_params_t *params,
    vec3_t x0,
    mat3_t P0
) {
    ukf->x = x0;
    ukf->P = P0;

    ukf->cell = params->cell;
    ukf->dt   = params->dt;
    ukf->Q    = params->Q;
    ukf->R    = params->R;

    float alpha1 = expf(-ukf->dt / ukf->cell.tau1);
    float alpha2 = expf(-ukf->dt / ukf->cell.tau2);

    ukf->A = (mat3_t){{
        {1.0f, 0.0f,   0.0f},
        {0.0f, alpha1, 0.0f},
        {0.0f, 0.0f,   alpha2},
    }};

    float b0 = -ukf->dt / (3600.0f * ukf->cell.Q);

    ukf->B = (vec3_t){
        b0,
        ukf->cell.r1 * (1.0f - alpha1),
        ukf->cell.r2 * (1.0f - alpha2)
    };

    /*
    $$
    \lambda = \alpha^2(n + \kappa) - n
    $$
    */
    ukf->lambda = params->alpha * params->alpha * (STATE_SIZE + params->kappa) - STATE_SIZE;

    SOC_UKF_mean_weights(ukf, ukf->mean_weights);
    SOC_UKF_covariance_weights(ukf, params);
}

static void SOC_UKF_sigma_points(
    const soc_ukf_t *ukf,
    vec3_t points[NUM_SIGMA_POINTS]
) {
    /*
    $$
    LL^T = (n + \lambda)P
    $$
    */
    float scaling_factor     = ukf->lambda + 3;
    mat3_t scaled_covariance = mat3_scale(&ukf->P, scaling_factor);
    mat3_t lower             = cholesky3(&scaled_covariance);

    // $\chi_0 = \hat{x}$
    points[0] = ukf->x;

    for (int i = 0; i < STATE_SIZE; i++) {
        vec3_t offset = mat3_column(&lower, i);

        // $\chi_{i+1} = \hat{x} + L_{:,i}$
        points[i + 1] = vec3_add(ukf->x, offset);

        // $\chi_{i+1+n} = \hat{x} - L_{:,i}$
        points[i + 1 + STATE_SIZE] = vec3_sub(ukf->x, offset);
    }
}

void SOC_UKF_predict(
    soc_ukf_t *ukf,
    float current
) {
    vec3_t points[NUM_SIGMA_POINTS];
    SOC_UKF_sigma_points(ukf, points);

    vec3_t Bu = vec3_scale(ukf->B, current);
    vec3_t predicted_points[NUM_SIGMA_POINTS];
    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        /*
        $$
        \chi_i^- = A\chi_i + BI
        $$
        */
        vec3_t Ax           = mat3_mult_vec3(&ukf->A, &points[i]);
        predicted_points[i] = vec3_add(Ax, Bu);
    }

    /*
    $$
    \hat{x}^{-} = \sum_{i=0}^{2n} W_i^{(m)}\chi_i^{-}
    $$
    */
    vec3_t predicted_mean = {0};
    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        vec3_t weighted_point = vec3_scale(predicted_points[i], ukf->mean_weights[i]);
        predicted_mean        = vec3_add(predicted_mean, weighted_point);
    }

    /*
    $$
    P^{-} = Q + \sum_{i=0}^{2n} W_i^{(c)}
    (\chi_i^{-} - \hat{x}^{-})
    (\chi_i^{-} - \hat{x}^{-})^T
    $$
    */
    mat3_t predicted_covariance = ukf->Q;

    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        vec3_t deviation      = vec3_sub(predicted_points[i], predicted_mean);
        mat3_t outer          = outer_product3(deviation, deviation);
        mat3_t weighted_outer = mat3_scale(&outer, ukf->covariance_weights[i]);

        predicted_covariance = mat3_add(&predicted_covariance, &weighted_outer);
    }

    ukf->x = predicted_mean;
    ukf->P = symmetrization3(&predicted_covariance);
}

void SOC_UKF_update(
    soc_ukf_t *ukf,
    float voltage,
    float current
) {
    vec3_t points[NUM_SIGMA_POINTS];
    SOC_UKF_sigma_points(ukf, points);

    float predicted_voltages[NUM_SIGMA_POINTS];

    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        float soc = points[i].x;
        float ocv = lut_lookup(&OCV_LUT, soc);
        float v0  = current * ukf->cell.r0;
        float v1  = points[i].y;
        float v2  = points[i].z;

        /*
        $$
        z_i = V_{\mathrm{OC}}(\mathrm{SOC}_i)
            - IR_0 - V_{1,i} - V_{2,i}
        $$
        */
        predicted_voltages[i] = ocv - v0 - v1 - v2;
    }

    /*
    $$
    \hat{z} = \sum_{i=0}^{2n} W_i^{(m)} z_i
    $$
    */
    float predicted_voltage = 0.0f;
    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        predicted_voltage += ukf->mean_weights[i] * predicted_voltages[i];
    }

    /*
    $$
    S = R + \sum_{i=0}^{2n} W_i^{(c)}
        (z_i - \hat{z})^2
    $$
    */
    float innovation_variance = ukf->R;

    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        float voltage_deviation = predicted_voltages[i] - predicted_voltage;

        innovation_variance +=
            ukf->covariance_weights[i]
            * voltage_deviation * voltage_deviation;
    }

    /*
    $$
    P_{xz} = \sum_{i=0}^{2n} W_i^{(c)}
        (\chi_i - \hat{x}^{-})(z_i - \hat{z})
    $$
    */
    vec3_t cross_covariance = {0};

    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        vec3_t state_deviation  = vec3_sub(points[i], ukf->x);
        float voltage_deviation = predicted_voltages[i] - predicted_voltage;

        vec3_t weighted_deviation = vec3_scale(
            state_deviation,
            ukf->covariance_weights[i] * voltage_deviation
        );

        cross_covariance = vec3_add(cross_covariance, weighted_deviation);
    }

    /*
    $$
    K = P_{xz} S^{-1}
    $$
    */
    vec3_t kalman_gain = vec3_scale(
        cross_covariance,
        1.0f / innovation_variance
    );

    /*
    $$
    \nu = V_{\mathrm{measured}} - \hat{z}
    $$
    */
    float innovation = voltage - predicted_voltage;

    /*
    $$
    \hat{x}^{+} = \hat{x}^{-} + K\nu
    $$
    */
    vec3_t correction = vec3_scale(kalman_gain, innovation);
    ukf->x            = vec3_add(ukf->x, correction);

    /*
    $$
    P^{+} = P^{-} - KSK^T
    $$
    */
    mat3_t gain_outer            = outer_product3(kalman_gain, kalman_gain);
    mat3_t covariance_correction = mat3_scale(&gain_outer, innovation_variance);
    mat3_t updated_covariance    = mat3_sub(&ukf->P, &covariance_correction);

    /*
    $$
    P^{+} \leftarrow \frac{1}{2}\left(P^{+} + (P^{+})^T\right)
    $$
    */
    ukf->P = symmetrization3(&updated_covariance);
}
