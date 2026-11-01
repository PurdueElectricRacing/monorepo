/**
 * @file main.h
 * @brief "Abox" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef MAIN_H
#define MAIN_H

#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/gpio/gpio.h"
#include "adbms.h"

// Status LEDs
#define HEARTBEAT_LED_PORT  (GPIOC)
#define HEARTBEAT_LED_PIN   (8)
#define ERROR_LED_PORT      (GPIOC)
#define ERROR_LED_PIN       (9)
#define CONNECTION_LED_PORT (GPIOC)
#define CONNECTION_LED_PIN  (7)

// SPI1 for BMS
#define SPI1_SCK_PORT  (GPIOA)
#define SPI1_MISO_PORT (GPIOA)
#define SPI1_MOSI_PORT (GPIOA)
#define SPI1_SCK_PIN   (5)
#define SPI1_MISO_PIN  (6)
#define SPI1_MOSI_PIN  (7)
#define SPI1_CS_PORT   (GPIOA)
#define SPI1_CS_PIN    (4)

// SPI2 for BMS
#define SPI2_SCK_PORT  (GPIOB)
#define SPI2_SCK_PIN   (13)
#define SPI2_MISO_PORT (GPIOB)
#define SPI2_MISO_PIN  (14)
#define SPI2_MOSI_PORT (GPIOB)
#define SPI2_MOSI_PIN  (15)
#define SPI2_CS_PORT   (GPIOC)
#define SPI2_CS_PIN    (6)

// BMS status LEDs
#define BMS_STATUS_IND_1_PORT (GPIOC)
#define BMS_STATUS_IND_1_PIN  (5)
#define BMS_STATUS_IND_2_PORT (GPIOB)
#define BMS_STATUS_IND_2_PIN  (0)
#define BMS_STATUS_IND_3_PORT (GPIOB)
#define BMS_STATUS_IND_3_PIN  (1)
#define BMS_STATUS_IND_4_PORT (GPIOB)
#define BMS_STATUS_IND_4_PIN  (2)
#define BMS_STATUS_IND_5_PORT (GPIOB)
#define BMS_STATUS_IND_5_PIN  (10)

// Analog current and voltage sense
#define ISENSE_GPIO_PORT   (GPIOC)
#define ISENSE_GPIO_PIN    (0)
#define ISENSE_ADC_CHANNEL (6)
#define VBATT_GPIO_PORT    (GPIOC)
#define VBATT_GPIO_PIN     (1)
#define VBATT_ADC_CHANNEL  (7)

// Input status pins
#define CHARGER_CONNECTED_PORT      (GPIOA)
#define CHARGER_CONNECTED_PIN       (0)
#define NOT_PRECHARGE_COMPLETE_PORT (GPIOA)
#define NOT_PRECHARGE_COMPLETE_PIN  (2)
#define IMD_STATUS_PORT             (GPIOC)
#define IMD_STATUS_PIN              (11)

// IMD PWM reading
#define IMD_PWM_LS_PORT             (GPIOC)
#define IMD_PWM_LS_PIN              (12)
#define IMD_PWM_LS_TIM              (TIM5)

// BMS SDC control
#define BMS_SDC_CTRL_PORT (GPIOA)
#define BMS_SDC_CTRL_PIN  (1)

typedef struct {
    uint16_t isense_raw;
    uint16_t vbatt_raw;
} adc1_dma_buffer_t;
extern volatile adc1_dma_buffer_t adc1_dma_buffer;

extern PHAL_ADC_Handle_t adc_handle;

extern adbms_bms_t g_bms;

#endif // MAIN_H
