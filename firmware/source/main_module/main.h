#ifndef MAIN_H
#define MAIN_H

/**
 * @file main.h
 * @brief "Main Module" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "common/phal_G4/gpio/gpio.h"

// Status LEDs
#define HEARTBEAT_LED_PORT  (GPIOC)
#define HEARTBEAT_LED_PIN   (7)
#define CONNECTION_LED_PORT (GPIOC)
#define CONNECTION_LED_PIN  (8)
#define ERROR_LED_PORT      (GPIOC)
#define ERROR_LED_PIN       (9)

// TSAL
#define TSAL_RED_CTRL_PORT   (GPIOC)
#define TSAL_RED_CTRL_PIN    (3)
#define TSAL_GREEN_CTRL_PORT (GPIOA)
#define TSAL_GREEN_CTRL_PIN  (0)

// Brake and Buzzer
#define BRAKE_LIGHT_PORT (GPIOB)
#define BRAKE_LIGHT_PIN  (0)
#define BUZZER_PORT      (GPIOB)
#define BUZZER_PIN       (1)

// MAIN SDC Control
#define ECU_SDC_CTRL_PORT (GPIOA)
#define ECU_SDC_CTRL_PIN  (5)

// SDC MUX (read + control)
#define SDC_MUX_PORT      (GPIOB)
#define SDC_MUX_PIN       (9)
#define SDC_MUX_S3_PORT   (GPIOB)
#define SDC_MUX_S3_PIN    (7)
#define SDC_MUX_S2_PORT   (GPIOB)
#define SDC_MUX_S2_PIN    (6)
#define SDC_MUX_S1_PORT   (GPIOB)
#define SDC_MUX_S1_PIN    (5)
#define SDC_MUX_S0_PORT   (GPIOB)
#define SDC_MUX_S0_PIN    (4)

// Digital input status pins
#define BMS_STATUS_PORT             (GPIOA)
#define BMS_STATUS_PIN              (7)
#define IMD_STATUS_PORT             (GPIOA)
#define IMD_STATUS_PIN              (6)
#define NOT_PRECHARGE_COMPLETE_PORT (GPIOC)
#define NOT_PRECHARGE_COMPLETE_PIN  (4)
// Analog voltage sense
#define VMC_PORT   (GPIOA)
#define VMC_PIN    (3)
#define VBATT_PORT (GPIOA)
#define VBATT_PIN  (4)

#endif // MAIN_H
