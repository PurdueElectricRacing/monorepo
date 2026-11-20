#include "g4_testing.h"

#if (G4_TESTING_CHOSEN == TEST_SOC_UKF)

#include "common/phal_G4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "soc_ukf.h"

static soc_ukf_t filter;
volatile float soc_estimate;

void HardFault_Handler();

static void soc_estimation_periodic(void);

RTOS_DEFINE_TASK(soc_estimation_periodic, 10, TASK_PRIORITY_NORMAL, 2048);

int main(void) {
    const vec3_t initial_state = {0.63f, 0.01f, -0.005f};
    const mat3_t initial_covariance = {{
        {0.0025f,  0.0002f,  -0.0001f},
        {0.0002f,  0.0004f,   0.00005f},
        {-0.0001f, 0.00005f,  0.0003f},
    }};

    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);
    SOC_UKF_init(&filter, &LV_PARAMS, initial_state, initial_covariance);

    RTOS_START_TASK(soc_estimation_periodic);
    vTaskStartScheduler();

    // Unreachable
    HardFault_Handler();
    return 0;
}

static void soc_estimation_periodic(void) {
    SOC_UKF_predict(&filter, 3.0f);
    SOC_UKF_update(&filter, 4.1f, 3.0f);
    soc_estimate = GET_SOC(&filter);
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_SOC_UKF
