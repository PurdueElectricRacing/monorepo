/**
* @file pwm_testing.c
* @brief Test file for PWM on STM32G4
* @author Natasha Pandit (npandit@purdue.edu)
*/

#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_PWM)

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pwm/pwm.h"
#include "common/utils/countof.h"

GPIOInitConfig_t gpio_config[] = {
    GPIO_INIT_AF(GPIOA, 8, 6, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
};

void HardFault_Handler(void);

/// Cycle through PWM duty-cycle values every two seconds.
static void pwm_update_2s(void);

FREERTOS_DEFINE_TASK(pwm_update_2s, 2'000, TASK_PRIORITY_HIGH, STACK_256);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);
    
    if(!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    if(!PHAL_PWM_init(TIM1, 1'000, 1)) {
        HardFault_Handler();
    }

    FREERTOS_START_TASK(pwm_update_2s);

    vTaskStartScheduler();

    HardFault_Handler();

    return 0;
}

static void pwm_update_2s(void) {
    /// Duty-cycle percentages applied sequentially by the test task.
    static constexpr uint8_t duty_cycles[] = {0, 25, 50, 75, 100,};

    static uint8_t duty_cycle_index = 0;

    PHAL_PWM_setPercent(TIM1, 1, duty_cycles[duty_cycle_index]);

    duty_cycle_index = (duty_cycle_index + 1) % countof(duty_cycles);
}

void HardFault_Handler(void) {
    __disable_irq();
    SysTick->CTRL = 0;

    while(1) {
        __asm__("nop");
    }
}

#endif