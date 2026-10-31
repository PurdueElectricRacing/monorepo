/**
 * @file pwm.c
 * @author Seth Baird (baird33@purdue.edu)
 * @date 2025-10-02
 */
 //credit to Eileen for writing most of this in pdu/fan_control.c

#include "common/phal_F4/pwm/pwm.h"
// #include "stm32f423xx.h"

extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;

//TIM6 and TIM7 do not support PWM!
bool PHAL_initPWM(uint32_t frequency_hz, TIM_TypeDef* tim, uint8_t channelsToEnable) {
    //cast to uint32 so switch statement can understand it as discrete values - its a uint32 in the background anyway
    switch ((uint32_t)tim) { 
        /* --- APB2 Timers --- */
        case (uint32_t)TIM1:
            RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
            break;

        case (uint32_t)TIM8:
            RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
            break;

        case (uint32_t)TIM9:
            RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;
            break;

        case (uint32_t)TIM10:
            RCC->APB2ENR |= RCC_APB2ENR_TIM10EN;
            break;

        case (uint32_t)TIM11:
            RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
            break;

        /* --- APB1 Timers --- */
        case (uint32_t)TIM2:
            RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
            break;

        case (uint32_t)TIM3:
            RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
            break;

        case (uint32_t)TIM4:
            RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
            break;

        case (uint32_t)TIM5:
            RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
            break;

        case (uint32_t)TIM12:
            RCC->APB1ENR |= RCC_APB1ENR_TIM12EN;
            break;

        case (uint32_t)TIM13:
            RCC->APB1ENR |= RCC_APB1ENR_TIM13EN;
            break;

        case (uint32_t)TIM14:
            RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
            break;

        case (uint32_t)TIM6:
        case (uint32_t)TIM7:
        default:
            //invalid or unsupported timer
            return false;
    }
    tim->CR1 &= ~TIM_CR1_CEN; // Disable counter (turn off timer)

    /* Solving for PSC:
     * PWMfreq = Fclk / ((ARR + 1) × (PSC + 1)).
     *
     * PWMfreq is known, and we are setting ARR to 99 for ease of calculations.
     *
     * Therefore we solve for the below formula for the desired PSC based on the
     * necessary PWMfreq specified by the fan vendor.
    */

    tim->ARR = 100 - 1; // Using this for ease of calculations

    
    if(tim == TIM1 || tim == TIM8 || tim == TIM9 || tim == TIM10 || tim == TIM11){
        tim->PSC = (APB2ClockRateHz / (frequency_hz * (tim->ARR + 1))) - 1;
    }else if(tim == TIM2 || tim == TIM5 || tim == TIM3 || tim == TIM4 || tim == TIM12 || tim == TIM13 || tim == TIM14){
        tim->PSC = (APB1ClockRateHz / (frequency_hz * (tim->ARR + 1))) - 1;
    }

    switch (channelsToEnable) {
        //cascades so channelsToEnable 4 means enable 1, 2, 3, 4
        //description of these register manipulations in case 1 block
        case 4:
            tim->CCR4 = 0;
            //uses CCMR2 for channels 3 and 4  (datasheet page 578)
            tim->CCMR2 &= ~TIM_CCMR2_OC4M; // Set PWM mode 1 (active while CNT <= CCR2)
            tim->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1;
            tim->CCMR2 |= TIM_CCMR2_OC4PE;
            tim->CCER |= TIM_CCER_CC4E;
        case 3:
            tim->CCR3 = 0;
            //uses CCMR2 for channels 3 and 4  (datasheet page 578)
            tim->CCMR2 &= ~TIM_CCMR2_OC3M;
            tim->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;
            tim->CCMR2 |= TIM_CCMR2_OC3PE;
            tim->CCER |= TIM_CCER_CC3E;
        case 2:
            tim->CCR2 = 0;
            tim->CCMR1 &= ~TIM_CCMR1_OC2M;
            tim->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;
            tim->CCMR1 |= TIM_CCMR1_OC2PE;
            tim->CCER |= TIM_CCER_CC2E;
        case 1:
            tim->CCR1 = 0; // Start with it off
            tim->CCMR1 &= ~TIM_CCMR1_OC1M; // Set PWM mode 1 (active while CNT <= CCR1)
            tim->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1; // PWM Mode 1 ch1
            tim->CCMR1 |= TIM_CCMR1_OC1PE; // Enable preload register ch1
            tim->CCER |= TIM_CCER_CC1E; // Enable output compare 1

            break;
        default: // only valid inputs are 1,2,3,4
            return false;
    }

    tim->BDTR |= TIM_BDTR_MOE; // Enable main output
    tim->CR1 &= ~TIM_CR1_DIR; // Set to upcounting mode
    tim->CR1 |= TIM_CR1_ARPE; // Necessary in upcounting mode
    tim->CR1 |= TIM_CR1_CEN; // Enable counter (turn on timer)
    tim->CR1 |= TIM_CR1_CEN; // Enable tim

    return true;
}

//no, channels are not zero-based
void PHAL_PWMsetPercent(TIM_TypeDef* tim, uint8_t channel, uint8_t percent) {
    if (percent > 100)
        percent = 100;

    if (percent < 0)
        percent = 0;

    switch (channel) {
        // Duty cycle is (CCR1 / ARR)%. So CCR1 = (ARR / duty cycle)
        // tim->CCR1 = (tim->ARR + 1) * (percent / 100.0); 
        // floating point vs integer math for efficiency vs saving cpu time, choosing the latter for this implementation since its just fans
        case 1:
            tim->CCR1 = ((tim->ARR + 1) * percent) / 100;
            break;
        case 2:
            tim->CCR2 = ((tim->ARR + 1) * percent) / 100;
            break;
        case 3:
            tim->CCR3 = ((tim->ARR + 1) * percent) / 100;
            break;
        case 4:
            tim->CCR4 = ((tim->ARR + 1) * percent) / 100;
            break;
    }
}
