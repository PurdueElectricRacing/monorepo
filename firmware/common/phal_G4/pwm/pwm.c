/**
* @file pwm.c
* @brief Public PWM driver interface for STM32G4
* @author Natasha Pandit (npandit@purdue.edu)
*/

#include <stddef.h>

#include "phal_G4/pwm/pwm.h"
#include "phal_G4/pwm/pwm_priv.h"

bool PHAL_PWM_init(TIM_TypeDef* tim, uint32_t frequency_hz, uint8_t channels_en) {
    if (tim == NULL || frequency_hz == 0U) {
        return false;
    }

    PWM_PRIV_TimerInfo_t timer_info;

    if (!PHAL_PWM_priv_getTimerInfo(tim, &timer_info)) {
        return false;
    }

    if (channels_en == 0U || channels_en > timer_info.max_channel) {
        return false;
    }

    /*
     * Keep ARR at 99 to provide 100 duty-cycle steps.
     *
     * frequency = timer_clock / ((ARR + 1) * (PSC + 1))
     */
    /// Auto-reload value providing 100 duty-cycle steps.
    const uint32_t auto_reload = 99U;
    /// Number of timer counts in one PWM period.
    const uint32_t period_steps = auto_reload + 1U;

    /*
     * Check for multiplication overflow before calculating
     * frequency_hz * period_steps.
     */
    if (frequency_hz > UINT32_MAX / period_steps) {
        return false;
    }

    /// Divisor needed to produce the requested PWM frequency.
    const uint32_t denominator = frequency_hz * period_steps;

    if (denominator == 0U || denominator > timer_info.timer_clock_hz) {
        return false;
    }

    /// Required timer clock divider.
    const uint32_t divider = timer_info.timer_clock_hz / denominator;

    if (divider == 0U || divider > 65536U) {
        return false;
    }

    /// Prescalar register value corresponding to the clock divider.
    const uint32_t prescaler = divider - 1U;

    return PHAL_PWM_priv_initTimer(tim, (uint16_t)prescaler, (uint16_t)auto_reload, channels_en, timer_info.requires_main_out_en);    
}

bool PHAL_PWM_setPercent(TIM_TypeDef *tim, uint8_t channel, uint8_t percent) {
    if (tim == NULL || channel < 1U || channel > 4U || percent > 100U) {
        return false;
    }

    /// Configured timer auto-reload value.
    const uint32_t auto_reload = PHAL_PWM_priv_getAutoReload(tim);

    /// Capture/compare value corresponding to requested duty cycle.
    const uint32_t compare_value = ((auto_reload + 1U) * percent) / 100U;

    return PHAL_PWM_priv_setCompare(tim, channel, compare_value);
}