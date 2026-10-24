/**
 * @file main.h
 * @brief "PDU" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef MAIN_H
#define MAIN_H

#include "common/phal_G4/gpio/gpio.h"

// Status indicators
#define HEARTBEAT_LED_PORT  (GPIOC)
#define HEARTBEAT_LED_PIN   (15)
#define CONNECTION_LED_PORT (GPIOC)
#define CONNECTION_LED_PIN  (14)
#define ERROR_LED_PORT      (GPIOC)
#define ERROR_LED_PIN       (13)

// Switch control lines
#define CTRL1_PORT  (GPIOA)
#define CTRL1_PIN   (9)
#define CTRL2_PORT  (GPIOA)
#define CTRL2_PIN   (8)
#define CTRL3_PORT  (GPIOC)
#define CTRL3_PIN   (9)
#define CTRL4_PORT  (GPIOA)
#define CTRL4_PIN   (10)
#define CTRL5_PORT  (GPIOC)
#define CTRL5_PIN   (8)
#define CTRL6_PORT  (GPIOB)
#define CTRL6_PIN   (15)
#define CTRL7_PORT  (GPIOC)
#define CTRL7_PIN   (6)
#define CTRL8_PORT  (GPIOC)
#define CTRL8_PIN   (7)
#define CTRL9_PORT  (GPIOC)
#define CTRL9_PIN   (10)
#define CTRL10_PORT (GPIOC)
#define CTRL10_PIN  (11)
#define CTRL11_PORT (GPIOB)
#define CTRL11_PIN  (14)
#define CTRL12_PORT (GPIOC)
#define CTRL12_PIN  (12)
#define CTRL13_PORT (GPIOD)
#define CTRL13_PIN  (2)
#define CTRL14_PORT (GPIOC)
#define CTRL14_PIN  (5)
#define CTRL15_PORT (GPIOB)
#define CTRL15_PIN  (1)
#define CTRL16_PORT (GPIOB)
#define CTRL16_PIN  (2)
#define CTRL17_PORT (GPIOB)
#define CTRL17_PIN  (10)
#define CTRL18_PORT (GPIOC)
#define CTRL18_PIN  (3)
#define CTRL19_PORT (GPIOC)
#define CTRL19_PIN  (2)
#define CTRL20_PORT (GPIOC)
#define CTRL20_PIN  (0)
#define CTRL21_PORT (GPIOB)
#define CTRL21_PIN  (11)

// Analog inputs
#define LVBATT_VS_PORT        (GPIOA)
#define LVBATT_VS_PIN         (3)
#define LVBATT_VS_ADC_CHANNEL (4)
#define LVBATT_CS_PORT        (GPIOB)
#define LVBATT_CS_PIN         (0)
#define LVBATT_CS_ADC_CHANNEL (12)
#define MUX1_OUT_PORT         (GPIOA)
#define MUX1_OUT_PIN          (5)
#define MUX1_OUT_ADC_CHANNEL  (13)
#define MUX2_OUT_PORT         (GPIOA)
#define MUX2_OUT_PIN          (4)
#define MUX2_OUT_ADC_CHANNEL  (17)

// Analog multiplexer controls
#define MUX1_CTRL_A_PORT (GPIOC) // todo flip this with MUX1_OUT in altium
#define MUX1_CTRL_A_PIN  (4)
#define MUX1_CTRL_B_PORT (GPIOA)
#define MUX1_CTRL_B_PIN  (6)
#define MUX1_CTRL_C_PORT (GPIOA)
#define MUX1_CTRL_C_PIN  (7)
#define MUX2_CTRL_A_PORT (GPIOC)
#define MUX2_CTRL_A_PIN  (1)
#define MUX2_CTRL_B_PORT (GPIOB)
#define MUX2_CTRL_B_PIN  (4)
#define MUX2_CTRL_C_PORT (GPIOA)
#define MUX2_CTRL_C_PIN  (15)
#define MUX2_CTRL_D_PORT (GPIOB)
#define MUX2_CTRL_D_PIN  (5)

typedef struct {
    uint16_t lvbatt_vs;
} raw_adc1_values_t;
extern volatile raw_adc1_values_t raw_adc1_values;

typedef struct {
    uint16_t mux1_out;
    uint16_t mux2_out;
} raw_adc2_values_t;
extern volatile raw_adc2_values_t raw_adc2_values;

typedef struct {
    uint16_t lvbatt_cs;
} raw_adc3_values_t;
extern volatile raw_adc3_values_t raw_adc3_values;


#endif // MAIN_H
