/**
* @file pwm_priv.c
* @brief Private STM32G4 PWM register implementation
* @author Natasha Pandit (npandit@purdue.edu)
*/

#include <stddef.h>

#include "common/phal_G4/pwm/pwm_priv.h"
#include "common/phal_G4/rcc/rcc.h"


/// Configure and enable a timer channel for PWM output.
static bool PHAL_PWM_priv_enableChannel(TIM_TypeDef *tim, uint8_t channel) {
    if (tim == NULL) {
        return false;
    }

    switch (channel) {
        case 1U:
            tim->CCMR1 &= ~TIM_CCMR1_OC1M_Msk;
            tim->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;

            tim->CCR1 = 0U;

            tim->CCMR1 |= TIM_CCMR1_OC1PE;
            tim->CCER |= TIM_CCER_CC1E;
            return true;

        case 2U:
            tim->CCMR1 &= ~TIM_CCMR1_OC2M_Msk;
            tim->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;

            tim->CCR2 = 0U;

            tim->CCMR1 |= TIM_CCMR1_OC2PE;
            tim->CCER |= TIM_CCER_CC2E;
            return true;

        case 3U:
            tim->CCMR2 &= ~TIM_CCMR2_OC3M_Msk;
            tim->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;

            tim->CCR3 = 0U;

            tim->CCMR2 |= TIM_CCMR2_OC3PE;
            tim->CCER |= TIM_CCER_CC3E;
            return true;

        case 4U:
            tim->CCMR2 &= ~TIM_CCMR2_OC4M_Msk;
            tim->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1;

            tim->CCR4 = 0U;

            tim->CCMR2 |= TIM_CCMR2_OC4PE;
            tim->CCER |= TIM_CCER_CC4E;
            return true;

        default:
            return false;
    }
}

bool PHAL_PWM_priv_getTimerInfo(const TIM_TypeDef *tim, PWM_PRIV_TimerInfo_t *info) {
    if (tim == NULL || info == NULL) {
        return false;
    }

    info->timer_clock_hz = 0U;
    info->max_channel = 0U;
    info->requires_main_out_en = false;

    switch ((uint32_t)tim) {
        /* APB2 timers */
        case (uint32_t)TIM1:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 4U;
            info->requires_main_out_en = true;
            return true;

        case (uint32_t)TIM8:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 4U;
            info->requires_main_out_en = true;
            return true;

        case (uint32_t)TIM15:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 2U;
            info->requires_main_out_en = true;
            return true;

        case (uint32_t)TIM16:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 1U;
            info->requires_main_out_en = true;
            return true;

        case (uint32_t)TIM17:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 1U;
            info->requires_main_out_en = true;
            return true;

        case (uint32_t)TIM20:
            info->timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
            info->max_channel = 4U;
            info->requires_main_out_en = true;
            return true;

        /* APB1 timers */
        case (uint32_t)TIM2:
        case (uint32_t)TIM3:
        case (uint32_t)TIM4:
        case (uint32_t)TIM5:
            info->timer_clock_hz = PHAL_RCC_getAPB1ClockHz();
            info->max_channel = 4U;
            return true;

        default:
            return false;
    }
}

bool PHAL_PWM_priv_enableTimerClock(const TIM_TypeDef *tim) {
    if (tim == NULL) {
        return false;
    }

    switch ((uint32_t)tim) {
        /* APB2 timers */
        case (uint32_t)TIM1:
            RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
            return true;

        case (uint32_t)TIM8:
            RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
            return true;

        case (uint32_t)TIM15:
            RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
            return true;

        case (uint32_t)TIM16:
            RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
            return true;

        case (uint32_t)TIM17:
            RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
            return true;

        case (uint32_t)TIM20:
            RCC->APB2ENR |= RCC_APB2ENR_TIM20EN;
            return true;

        /* APB1 timers */
        case (uint32_t)TIM2:
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
            return true;

        case (uint32_t)TIM3:
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
            return true;

        case (uint32_t)TIM4:
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
            return true;

        case (uint32_t)TIM5:
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
            return true;

        default:
            return false;
    }
}

bool PHAL_PWM_priv_initTimer(TIM_TypeDef *tim, uint16_t prescaler, uint16_t auto_reload, uint8_t channels_en, bool requires_main_out_en) {
    if (tim == NULL || channels_en == 0U || channels_en > 4U) {
        return false;
    }

    if (!PHAL_PWM_priv_enableTimerClock(tim)) {
        return false;
    }

    /*
     * Stop the timer while its configuration registers are modified.
     */
    tim->CR1 &= ~TIM_CR1_CEN;

    tim->PSC = prescaler;
    tim->ARR = auto_reload;

    /*
     * Configure consecutive PWM channels beginning with channel 1.
     */
    for (uint8_t channel = 1U; channel <= channels_en; channel++) {
        if (!PHAL_PWM_priv_enableChannel(tim, channel)) {
            return false;
        }
    }

    /*
     * Advanced-control timers require the main output enable bit
     * before their outputs can appear on the GPIO pins.
     */
    if (requires_main_out_en) {
        tim->BDTR |= TIM_BDTR_MOE;
    }

    /*
     * Configure the timer for up-counting and enable ARR preload.
     */
    tim->CR1 &= ~TIM_CR1_DIR;
    tim->CR1 |= TIM_CR1_ARPE;

    /*
     * Reset the counter and load the new PSC and ARR values.
     */
    tim->CNT = 0U;
    tim->EGR |= TIM_EGR_UG;

    /*
     * Start the timer.
     */
    tim->CR1 |= TIM_CR1_CEN;

    return true;
}

bool PHAL_PWM_priv_setCompare(TIM_TypeDef *tim, uint8_t channel, uint32_t compare_value) {
    if (tim == NULL) {
        return false;
    }

    switch (channel) {
        case 1U:
            tim->CCR1 = compare_value;
            return true;

        case 2U:
            tim->CCR2 = compare_value;
            return true;

        case 3U:
            tim->CCR3 = compare_value;
            return true;

        case 4U:
            tim->CCR4 = compare_value;
            return true;

        default:
            return false;
    }
}

uint32_t PHAL_PWM_priv_getAutoReload(const TIM_TypeDef *tim) {
    if (tim == NULL) {
        return 0U;
    }

    return tim->ARR;
}
