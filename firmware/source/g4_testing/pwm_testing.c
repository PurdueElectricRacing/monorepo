/**
 * @file pwm_testing.c
 * @brief Test file for PWM on STM32G4
 * @author Natasha Pandit (npandit@purdue.edu)
 * @author Krish Sinha (sinha239@purdue.edu)
 */

#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_PWM)

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pwm/pwm.h"
#include "common/utils/countof.h"

// Designed to test multiple GPIO pins on PDU for PER27
//When you are configuring new pins use page 73 for the alternate function mapping for the STM32G474RET6: 
//https://www.st.com/resource/en/datasheet/stm32g474cb.pdf
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Port A pins on Timer 2 (AF1)
    PHAL_GPIO_INIT_AF(GPIOA, 0, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_AF(GPIOA, 1, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_AF(GPIOA, 2, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    
    // Port B pins on Timer 4 (AF2)
    PHAL_GPIO_INIT_AF(GPIOB, 6, 2, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_AF(GPIOB, 7, 2, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_AF(GPIOB, 9, 2, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
};

void HardFault_Handler(void);

/// Cycle through PWM duty-cycle values every two seconds.
static void pwm_update_2s(void);

RTOS_DEFINE_TASK(pwm_update_2s, 2'000, TASK_PRIORITY_HIGH, STACK_256);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);
    
    if(!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Enable up to Channel 3 on TIM2
    if(!PHAL_PWM_init(TIM2, 1'000, 3)) {
        HardFault_Handler();
    }

    // Enable up to Channel 4 on TIM4
    if(!PHAL_PWM_init(TIM4, 1'000, 4)) {
        HardFault_Handler();
    }

    RTOS_START_TASK(pwm_update_2s);

    vTaskStartScheduler();

    HardFault_Handler();

    return 0;
}

static void pwm_update_2s(void) {
    /// Duty-cycle percentages applied sequentially by the test task.
    static constexpr uint8_t duty_cycles[] = {0, 25, 50, 75, 100,};

    static uint8_t duty_cycle_index = 0;

    // Update Timer 2 channels (PA0, PA1, PA2)
    PHAL_PWM_setPercent(TIM2, 1, duty_cycles[duty_cycle_index]); // PA0
    PHAL_PWM_setPercent(TIM2, 2, duty_cycles[duty_cycle_index]); // PA1
    PHAL_PWM_setPercent(TIM2, 3, duty_cycles[duty_cycle_index]); // PA2

    // Update Timer 4 channels (PB6, PB7, PB9)
    PHAL_PWM_setPercent(TIM4, 1, duty_cycles[duty_cycle_index]); // PB6
    PHAL_PWM_setPercent(TIM4, 2, duty_cycles[duty_cycle_index]); // PB7
    PHAL_PWM_setPercent(TIM4, 4, duty_cycles[duty_cycle_index]); // PB9

    duty_cycle_index = (uint8_t)((duty_cycle_index + 1) % countof(duty_cycles));
}

void HardFault_Handler(void) {
    __disable_irq();
    SysTick->CTRL = 0;

    while(1) {
        __asm__("nop");
    }
}

#endif