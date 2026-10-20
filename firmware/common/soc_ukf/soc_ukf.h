#include <math.h>

#include "../linear_algebra/linear_algebra.h"

typedef struct {
    vec3_t x; // [SoC, V1, V2]
    mat3_t A; // state transition
    vec3_t B; // control matrix
    mat3_t Q; // process noise covariance
    float R;  // measurement noise covariance
} soc_ukf_t;

typedef struct {
    float r0;   // ohms
    float r1;   // ohms
    float r2;   // ohms
    float tau1; // seconds
    float tau2; // seconds
    float Q;    // Ah
} cell_params_t;

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

void SOC_UKF_init(soc_ukf_t* ukf) {
    // todo init SOC0 using OCV lookup
    ukf->x = (vec3_t){0.0f, 0.0f, 0.0f};

    float alpha1 = expf(-DT / RS50_PARAMS.tau1);
    float alpha2 = expf(-DT / RS50_PARAMS.tau2);

    ukf->A = (mat3_t)
    {{{1.0f, 0.0f,   0.0f},
      {0.0f, alpha1, 0.0f},
      {0.0f, 0.0f,   alpha2}}};

    float b0 = -DT / (3600.0f * RS50_PARAMS.Q);
    
    ukf->B = (vec3_t){b0, 1.0f - alpha1, 1.0f - alpha2};

    // todo init Q and R
}

vec3_t SOC_UKF_predict(const soc_ukf_t* ukf, float current) {
    vec3_t Ax = mat3_mult_vec3(&ukf->A, &ukf->x);
    vec3_t Bu = vec3_scale(ukf->B, current);

    return vec3_add(Ax, Bu);
}

void SOC_UKF_update(soc_ukf_t* ukf, float voltage) {
    // todo
}

