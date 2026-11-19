#include <math.h>

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
    vec3_t x; // [SoC, V1, V2]
    mat3_t P; // state covariance

    const mat3_t A; // state transition
    const vec3_t B; // control matrix

    const mat3_t Q; // process noise covariance
    const float R;  // measurement noise covariance

    const cell_params_t cell;
    const float dt;

    const float alpha;
    const float beta;
    const float kappa;
    const float lambda;

    const float mean_weights[NUM_SIGMA_POINTS];
    const float covariance_weights[NUM_SIGMA_POINTS];
} soc_ukf_t;


// placeholder
static constexpr cell_params_t RS50_PARAMS = {
    .r0   = 0.015f,
    .r1   = 0.015f,
    .r2   = 0.015f,
    .tau1 = 100.0f,
    .tau2 = 1000.0f,
    .Q    = 5.0f
};

static constexpr float DT = 0.01f;


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

void SOC_UKF_init(soc_ukf_t *ukf) {
    // todo init SOC0 using OCV lookup
    ukf->x = (vec3_t){0.0f, 0.0f, 0.0f};

    float alpha1 = expf(-DT / RS50_PARAMS.tau1);
    float alpha2 = expf(-DT / RS50_PARAMS.tau2);

    ukf->A = (mat3_t){{
        {1.0f, 0.0f,   0.0f},
        {0.0f, alpha1, 0.0f},
        {0.0f, 0.0f,   alpha2},
    }};

    float b0 = -DT / (3600.0f * RS50_PARAMS.Q);

    ukf->B = (vec3_t){
        b0,
        1.0f - alpha1,
        1.0f - alpha2
    };

    // todo split into LV and HV params
    ukf->Q = (mat3_t){{
        {1e-6f, 0.0f,  0.0f},
        {0.0f,  1e-6f, 0.0f},
        {0.0f,  0.0f,  1e-6f},
    }};

    ukf->R = 1e-3f;

    ukf->alpha = 1.0f;
    ukf->kappa = 0.0f;

    /*
    $$
    \lambda = \alpha^2(n + \kappa) - n
    $$
    */
    ukf->lambda = ukf->alpha * ukf->alpha * (STATE_SIZE + ukf->kappa) - STATE_SIZE;

    SOC_UKF_mean_weights(ukf, ukf->mean_weights);
}

void SOC_UKF_covariance_weights(
    const soc_ukf_t *ukf
) {
    // todo
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
