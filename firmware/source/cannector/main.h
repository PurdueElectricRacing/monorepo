/**
 * @file main.h
 * @brief "CANnector" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef MAIN_H
#define MAIN_H

#include "common/rtos/rtos.h"

#define CONNECTION_LED_PORT (GPIOA)
#define CONNECTION_LED_PIN  (5)
#define HEARTBEAT_LED_PORT  (GPIOA)
#define HEARTBEAT_LED_PIN   (6)
#define ERROR_LED_PORT      (GPIOA)
#define ERROR_LED_PIN       (7)

#define VCAN_LED_PORT (GPIOC)
#define VCAN_LED_PIN  (4)
#define MCAN_LED_PORT (GPIOC)
#define MCAN_LED_PIN  (5)
#define SCAN_LED_PORT (GPIOB)
#define SCAN_LED_PIN  (0)
#define USB_LED_PORT  (GPIOB)
#define USB_LED_PIN   (1)

#define CHARGER_PORT (GPIOB)
#define CHARGER_PIN  (2)

extern QueueHandle_t can_queue;

#endif // MAIN_H