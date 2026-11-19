#include "soc_ukf.h"

#include <math.h>

#include "../lerp_lut/lerp_lut.h"

// todo replace
// todo add 2d lookup + temperature
lut_entry_t ocv_entries[11] = {
    {0.0f, 3.0f},
    {0.1f, 3.2f},
    {0.2f, 3.4f},
    {0.3f, 3.6f},
    {0.4f, 3.8f},
    {0.5f, 4.0f},
    {0.6f, 4.1f},
    {0.7f, 4.2f},
    {0.8f, 4.3f},
    {0.9f, 4.4f},
    {1.0f, 4.5f}
};
LERP_LUT_INIT(OCV_LUT, ocv_entries, 11);

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
    W_0^{(c)} = W_0^{(m)} + 1 - \alpha^2 + \beta,
    \qquad W_i^{(c)} = W_i^{(m)} \quad (i > 0)
    $$
    */
    ukf->covariance_weights[0] = ukf->mean_weights[0]
        + 1.0f - params->alpha * params->alpha + params->beta;
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
    float scaling_factor = ukf->lambda + 3;
    mat3_t scaled_covariance = mat3_scale(&ukf->P, scaling_factor);
    mat3_t lower = cholesky3(&scaled_covariance);

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
    float voltage
) {
    // todo
}
