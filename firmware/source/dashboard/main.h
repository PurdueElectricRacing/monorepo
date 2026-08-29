/**
 * @file main.c
 * @brief "Dashboard" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Chris Mcgalliard (cpmcgalliard@gmail.com)
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/gpio/gpio.h"

typedef volatile struct {
    uint16_t throttle1;
    uint16_t throttle2;
    uint16_t regen1;
    uint16_t regen2;
    uint16_t brake1_pressure;
    uint16_t brake2_pressure;
} raw_adc_values_t;
extern volatile raw_adc_values_t raw_adc_values;

extern PHAL_ADC_Handle_t adc_handle;

// On-board Status LEDs
#define CONNECTION_LED_PORT (GPIOB)
#define CONNECTION_LED_PIN  (7)
#define HEARTBEAT_LED_PORT  (GPIOB)
#define HEARTBEAT_LED_PIN   (9)
#define ERROR_LED_PORT      (GPIOD)
#define ERROR_LED_PIN       (2)

// External LEDs
#define IMD_LED_PORT   (GPIOA)
#define IMD_LED_PIN    (7)
#define BMS_LED_PORT   (GPIOA)
#define BMS_LED_PIN    (6)
#define PRCHG_LED_PORT (GPIOC)
#define PRCHG_LED_PIN  (4)
#define REGEN_LED_PORT (GPIOC)
#define REGEN_LED_PIN  (5)

// Main Button inputs
#define UP_BUTTON_PORT     (GPIOC)
#define UP_BUTTON_PIN      (6)
#define DOWN_BUTTON_PORT   (GPIOC)
#define DOWN_BUTTON_PIN    (7)
#define RIGHT_BUTTON_PORT  (GPIOC)
#define RIGHT_BUTTON_PIN   (8)
#define LEFT_BUTTON_PORT   (GPIOC)
#define LEFT_BUTTON_PIN    (9)
#define START_BUTTON_PORT  (GPIOB)
#define START_BUTTON_PIN   (14)
#define SELECT_BUTTON_PORT (GPIOB)
#define SELECT_BUTTON_PIN  (15)

// Steering Wheel Button inputs
#define REGEN_TOGGLE_PORT (GPIOA)
#define REGEN_TOGGLE_PIN  (4)
#define MARK_DATA_PORT    (GPIOA)
#define MARK_DATA_PIN     (5)
#define EBB_MINUS_PORT    (GPIOB)
#define EBB_MINUS_PIN     (0)
#define EBB_PLUS_PORT     (GPIOB)
#define EBB_PLUS_PIN      (1)
#define TV1_PLUS_PORT     (GPIOB)
#define TV1_PLUS_PIN      (11)
#define TV1_MINUS_PORT    (GPIOB)
#define TV1_MINUS_PIN     (13)
#define LAP_BUTTON_PORT   (GPIOB)
#define LAP_BUTTON_PIN    (14)


// Throttle
#define THROTTLE1_PORT        (GPIOC) // THROTTLE
#define THROTTLE1_PIN         (2)
#define THROTTLE1_ADC_CHANNEL (8)
#define THROTTLE2_PORT        (GPIOC) // THROTTLE_INV
#define THROTTLE2_PIN         (1)
#define THROTTLE2_ADC_CHANNEL (7)

// Brake
#define REGEN1_PORT        (GPIOC)
#define REGEN1_PIN         (3)
#define REGEN1_ADC_CHANNEL (9)
#define REGEN2_PORT        (GPIOA)
#define REGEN2_PIN         (0)
#define REGEN2_ADC_CHANNEL (1)

// Brake Pressure
#define BRAKE1_PRESSURE_PORT        (GPIOA)
#define BRAKE1_PRESSURE_PIN         (1)
#define BRAKE1_PRESSURE_ADC_CHANNEL (2)
#define BRAKE2_PRESSURE_PORT        (GPIOA)
#define BRAKE2_PRESSURE_PIN         (2)
#define BRAKE2_PRESSURE_ADC_CHANNEL (3)

#endif // MAIN_H
