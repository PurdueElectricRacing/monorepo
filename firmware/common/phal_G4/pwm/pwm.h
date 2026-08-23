/**
 * @file pwm.h
 * @brief PWM driver for STM32G4
 * @author Natasha Pandit (npandit@purdue.edu)
 */

#ifndef PHAL_PWM_H
#define PHAL_PWM_H

#include "stm32g474xx.h"

#include <stdbool.h>
#include <stdint.h>

/**
* @brief Initialize consecutive PWM channels beginning at channel 1.
*
* Configures the selected timer to generate PWM at the requested frequency and
* enables channels 1 through @p channels_en.
*
* @param tim Timer peripheral.
* @param frequency_hz Requested PWM frequency.
* @param channels_en Channels to enable.
*
* @return true if initialized properly; false if tim is NULL, frequency_hz is 0, or initialization fails.
*/
bool PHAL_PWM_init(TIM_TypeDef* tim, uint32_t frequency_hz, uint8_t channels_en);

/**
* @brief Set a PWM channel's duty cycle.
*
* @param tim Timer peripheral.
* @param channel Specific channel to set duty cycle of.
* @param percent Duty cycle percentage to set, 0-100.
*
* @return true if duty cycle is set properly; false if tim is NULL or setting cycle fails. 
*/
bool PHAL_PWM_setPercent(TIM_TypeDef* tim, uint8_t channel, uint8_t percent);

#endif
