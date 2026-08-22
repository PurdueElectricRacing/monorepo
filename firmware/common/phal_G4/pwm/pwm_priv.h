/**
* @file pwm_priv.h
* @brief Header file for private STM32G4 PWM register implementation
* @author Natasha Pandit (npandit@purdue.edu)
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx.h"

/// Timer-specific information required to configure PWM.
typedef struct {
    uint32_t timer_clock_hz; /// Timer input clock frequency in Hz.
    uint8_t max_channel; /// Highest PWM channel supported by timer.
    bool requires_main_out_en; /// Whether the timer requires main output enable bit.
} PWM_PRIV_TimerInfo_t;

/// Get timer specific information.
bool PHAL_PWM_priv_getTimerInfo(const TIM_TypeDef *tim, PWM_PRIV_TimerInfo_t *info);

/// Enable the peripheral clock for a timer.
bool PHAL_PWM_priv_enableTimerClock(const TIM_TypeDef *tim);

/// Configure timer registers for PWM operation.
bool PHAL_PWM_priv_initTimer(TIM_TypeDef *tim, uint16_t prescaler, uint16_t auto_reload, uint8_t channels_en, bool requires_main_out_en);

/// Set the capture/compare value for a PWM channel.
bool PHAL_PWM_priv_setCompare(TIM_TypeDef *tim, uint8_t channel, uint32_t compare_value);

/// Get the timer auto-reload register value.
uint32_t PHAL_PWM_priv_getAutoReload(const TIM_TypeDef *tim);
