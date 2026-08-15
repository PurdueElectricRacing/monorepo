/**
 * @file main.h
 * @brief "DAQ" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Luke Oxley (lcoxley@purdue.edu)
 */

#ifndef MAIN_H
#define MAIN_H

#include "common/rtos/rtos.h"
#include "sd_card.h"
#include "spmc.h"

// Pinouts
// LEDs
#define HEARTBEAT_LED_PORT  (GPIOD)
#define HEARTBEAT_LED_PIN   (13)
#define CONNECTION_LED_PORT (GPIOD)
#define CONNECTION_LED_PIN  (14)
#define ERROR_LED_PORT      (GPIOD)
#define ERROR_LED_PIN       (15)

// SD Card SDIO
#define SD_ACTIVITY_LED_PORT (GPIOA)
#define SD_ACTIVITY_LED_PIN  (9)
#define SD_ERROR_LED_PORT    (GPIOA)
#define SD_ERROR_LED_PIN     (8)
#define SD_DETECT_LED_PORT   (GPIOA)
#define SD_DETECT_LED_PIN    (10)
#define SD_CD_PORT           (GPIOD)
#define SD_CD_PIN            (4)

// W5500 ETH SPI1
#define ETH_CS_PORT   (GPIOA)
#define ETH_CS_PIN    (4)
#define ETH_SCK_PORT  (GPIOA)
#define ETH_SCK_PIN   (5)
#define ETH_MISO_PORT (GPIOA)
#define ETH_MISO_PIN  (6)
#define ETH_MOSI_PORT (GPIOA)
#define ETH_MOSI_PIN  (7)
#define ETH_RST_PORT  (GPIOE)
#define ETH_RST_PIN   (3)

// MISC
#define PWR_LOSS_PORT   (GPIOE)
#define PWR_LOSS_PIN    (15)
#define LOG_ENABLE_PORT (GPIOC)
#define LOG_ENABLE_PIN  (15)

#define PER   1
#define GREAT PER
static_assert(PER == GREAT); // Long live daq loop

#define SD_BLOCKING_TIMEOUT_MS (5000)
constexpr TickType_t SD_BLOCKING_TIMEOUT_TICKS = pdMS_TO_TICKS(SD_BLOCKING_TIMEOUT_MS); 

extern SemaphoreHandle_t spi1_lock;
extern volatile TaskHandle_t sd_task_handle; // ! handle to be pointed to by SDIO ISR for notifications

void HardFault_Handler();

#endif // MAIN_H
