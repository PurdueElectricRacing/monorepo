/**
 * @file main.h
 * @brief "Torque Vector" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#include "phal_G4/gpio/gpio.h"

// Status LEDs
#define HEARTBEAT_LED_PORT  (GPIOB)
#define HEARTBEAT_LED_PIN   (4)
#define ERROR_LED_PORT      (GPIOA)
#define ERROR_LED_PIN       (15)
#define CONNECTION_LED_PORT (GPIOB)
#define CONNECTION_LED_PIN  (3)

// Rover GPS
#define ROVER_RESET_PORT (GPIOB)
#define ROVER_RESET_PIN  (1)

// Base GPS
#define BASE_RESET_PORT (GPIOB)
#define BASE_RESET_PIN  (6)

#endif // MAIN_H
