//testing playground for pwm hal on f4 devboard (cannot just use pdu code)

#include "common/phal_F4/pwm/pwm.h"
#include "common/phal_F4/gpio/gpio.h"
#include "f4_testing.h"


#if (F4_TESTING_CHOSEN == TEST_PWM)

#include "common/rtos/rtos.h"
#include "common/phal_F4/gpio/gpio.h"
#include "common/phal_F4/rcc/rcc.h"
#include "common/utils/countof.h"

GPIOInitConfig_t gpio_config[] = {

    // TIM1 AF1
    GPIO_INIT_AF(GPIOA, 8, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOA, 9, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOA, 10, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOA, 11, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    //TIM2
    //there is no CH1
    GPIO_INIT_AF(GPIOB, 3, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOB, 10, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOB, 11, 1, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    // TIM8 AF3
    GPIO_INIT_AF(GPIOC, 6, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOC, 7, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOC, 8, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOC, 9, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    //TIM9 BP AF3
    GPIO_INIT_AF(GPIOA, 2, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(GPIOA, 3, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    //TIM10 BP AF3
    GPIO_INIT_AF(GPIOB, 8, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    //TIM11 BP AF3
    GPIO_INIT_AF(GPIOB, 9, 3, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

};

extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;
extern uint32_t AHBClockRateHz;
extern uint32_t PLLClockRateHz;

#define TargetCoreClockrateHz 16000000
ClockRateConfig_t   clock_config = {
    .clock_source              = CLOCK_SOURCE_HSI,
    .use_pll                   = false,
    .vco_output_rate_target_hz = 160000000,
    .system_clock_target_hz    = TargetCoreClockrateHz,
    .ahb_clock_target_hz       = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz      = (TargetCoreClockrateHz / (1)),
    .apb2_clock_target_hz      = (TargetCoreClockrateHz / (1)),
};

void HardFault_Handler();
void oldtim();


int main() {
    // Initialize hardware
    if (0 != PHAL_configureClockRates(&clock_config)) {
        HardFault_Handler();
    }
    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    //TIM1
    // PHAL_initPWM(25000, TIM1, 2);
    // PHAL_PWMsetPercent(TIM1, 1, 20);
    // PHAL_PWMsetPercent(TIM1, 2, 40);

    //TIM2 
    // PHAL_initPWM(25000, TIM2, 4);
    // //there is no CH1 on disco
    // PHAL_PWMsetPercent(TIM2, 2, 70);
    // PHAL_PWMsetPercent(TIM2, 3, 20);
    // PHAL_PWMsetPercent(TIM2, 4, 30);

    //TIM8 (discovery only)
    // PHAL_initPWM(25000, TIM8, 4);
    // PHAL_PWMsetPercent(TIM8, 1, 20);
    // PHAL_PWMsetPercent(TIM8, 2, 70);
    // PHAL_PWMsetPercent(TIM8, 3, 80);
    // PHAL_PWMsetPercent(TIM8, 4, 30);

    //TIM9 ch1, ch2 testing
    // PHAL_initPWM(25000, TIM9, 2);
    // PHAL_PWMsetPercent(TIM9, 1, 30);
    // PHAL_PWMsetPercent(TIM9, 2, 70);

    //TIM10 ch1 testing
    // PHAL_initPWM(25000, TIM10, 1);
    // PHAL_PWMsetPercent(TIM10, 1, 65);
    
    //TIM11 ch1 testing
    // PHAL_initPWM(25000, TIM11, 1);
    // PHAL_PWMsetPercent(TIM11, 1, 30);
    
    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // F4_TESTING_CHOSEN == TEST_PWM