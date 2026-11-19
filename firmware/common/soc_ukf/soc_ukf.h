#include <math.h>

#include "../linear_algebra/linear_algebra.h"
#include "../lerp_lut/lerp_lut.h"
#include "../utils/countof.h"

constexpr int STATE_SIZE = 3;
constexpr int NUM_SIGMA_POINTS = 2 * STATE_SIZE + 1; // 2n + 1

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
    .Q = {{
        {1e-6f, 0.0f,  0.0f},
        {0.0f,  1e-6f, 0.0f},
        {0.0f,  0.0f,  1e-6f},
    }},
    .R = 1e-3f,
    .alpha = 1.0f,
    .beta = 2.0f,
    .kappa = 0.0f,
};


void SOC_UKF_mean_weights(
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

void SOC_UKF_covariance_weights(
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
    ukf->x    = x0;
    ukf->P    = P0;

    ukf->cell  = params->cell;
    ukf->dt    = params->dt;
    ukf->Q     = params->Q;
    ukf->R     = params->R;

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

void SOC_UKF_sigma_points(
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

void SOC_UKF_predict(const soc_ukf_t *ukf, float current) {
    vec3_t points[NUM_SIGMA_POINTS];
    SOC_UKF_sigma_points(ukf, points);

    vec3_t predicted_points[NUM_SIGMA_POINTS];
    for (int i = 0; i < NUM_SIGMA_POINTS; i++) {
        /*
        $$
        \chi_i^- = A\chi_i + BI
        $$
        */
        vec3_t Ax = mat3_mult_vec3(&ukf->A, &points[i]);
        vec3_t Bu = vec3_scale(ukf->B, current);
        predicted_points[i] = vec3_add(Ax, Bu);
    }


}

void SOC_UKF_update(
    soc_ukf_t *ukf,
    float voltage
) {
    // todo
}
