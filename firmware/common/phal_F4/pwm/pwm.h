/**
 * @file pwm.h
 * @author Seth Baird (baird33@purdue.edu)
 * @date 2025-10-02
 */

#ifndef _PHAL_PWM_H
#define _PHAL_PWM_H

#include "phal_F4/phal_F4.h"
#include "phal_F4/gpio/gpio.h"

bool PHAL_initPWM(uint32_t frequency_hz, TIM_TypeDef* tim, uint8_t channelsToEnable);
void PHAL_PWMsetPercent(TIM_TypeDef* tim, uint8_t channel, uint8_t percent);

#endif