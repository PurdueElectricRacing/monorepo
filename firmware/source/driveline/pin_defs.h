/**
 * @file pin_defs.h
 * @brief "Driveline" node pin definitions and related constants
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include "phal_G4/gpio/gpio.h"

// Status LEDs
#define HEARTBEAT_LED_PORT  (GPIOA)
#define HEARTBEAT_LED_PIN   (15)
#define ERROR_LED_PORT      (GPIOB)
#define ERROR_LED_PIN       (4)
#define CONNECTION_LED_PORT (GPIOB)
#define CONNECTION_LED_PIN  (3)

// Shockpots
#define SHOCKPOT_LEFT_GPIO_PORT  (GPIOB)
#define SHOCKPOT_LEFT_GPIO_PIN   (13)
#define SHOCKPOT_LEFT_ADC_CHNL   (5)

#define SHOCKPOT_RIGHT_GPIO_PORT (GPIOB)
#define SHOCKPOT_RIGHT_GPIO_PIN  (12)
#define SHOCKPOT_RIGHT_ADC_CHNL  (3)

// Oil Temps
#define OIL_TEMP_L_GPIO_Port (GPIOA)
#define OIL_TEMP_L_Pin       (2)
#define OIL_TEMP_L_ADC_CH    (3)

#define OIL_TEMP_R_GPIO_Port (GPIOA)
#define OIL_TEMP_R_Pin       (1)
#define OIL_TEMP_R_ADC_CH    (2)

#endif // PIN_DEFS_H
