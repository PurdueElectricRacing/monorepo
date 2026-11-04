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

// Pinouts
// LEDs (HB, CAN, ERR)
#define HEARTBEAT_LED_PORT  (GPIOB)
#define HEARTBEAT_LED_PIN   (5)
#define VCAN_LED_PORT       (GPIOB)
#define VCAN_LED_PIN        (8)
#define MCAN_LED_PORT       (GPIOB)
#define MCAN_LED_PIN        (9)
#define SCAN_LED_PORT       (GPIOB)
#define SCAN_LED_PIN        (7)
#define ERROR_LED_PORT      (GPIOB)
#define ERROR_LED_PIN       (6)

// SD Card Status LEDs 
#define SD_ACTIVITY_LED_PORT (GPIOB)
#define SD_ACTIVITY_LED_PIN  (0)
#define SD_ERROR_LED_PORT    (GPIOC)
#define SD_ERROR_LED_PIN     (5)
#define SD_DETECT_LED_PORT   (GPIOC)
#define SD_DETECT_LED_PIN    (1)
#define SD_CD_PORT           (GPIOC)
#define SD_CD_PIN            (1)

// SD Card SPI
#define SDIO_CLK_PORT  (GPIOB)
#define SDIO_CLK_PIN   (15)
#define SDIO_CMD_PORT  (GPIOA)
#define SDIO_CMD_PIN   (6)
#define SDIO_DAT0_PORT (GPIOC)
#define SDIO_DAT0_PIN  (8)
#define SDIO_DAT1_PORT (GPIOC)
#define SDIO_DAT1_PIN  (9)
#define SDIO_DAT2_PORT (GPIOA)
#define SDIO_DAT2_PIN  (9)
#define SDIO_DAT3_PORT (GPIOC)
#define SDIO_DAT3_PIN  (11)

// W5500 ETH SPI1
// CS should be renamed to NSS imo but its also not that deep
#define ETH_CS_PORT   (GPIOA)
#define ETH_CS_PIN    (4)
#define ETH_SCK_PORT  (GPIOA)
#define ETH_SCK_PIN   (5)
#define ETH_MISO_PORT (GPIOB)
#define ETH_MISO_PIN  (4)
#define ETH_MOSI_PORT (GPIOA)
#define ETH_MOSI_PIN  (7)
#define ETH_RST_PORT  (GPIOC)
#define ETH_RST_PIN   (3)
#define ETH_nINT_PORT (GPIOC)
#define ETH_nINT_PIN  (2)

// CAN
#define CAN1_RX_PORT (GPIOA)
#define CAN1_RX_PIN  (11)
#define CAN1_TX_PORT (GPIOA)
#define CAN1_TX_PIN  (12)
#define CAN2_RX_PORT (GPIOB)
#define CAN2_RX_PIN  (12)
#define CAN2_TX_PORT (GPIOB)
#define CAN2_TX_PIN  (13)
#define CAN3_RX_PORT (GPIOA)
#define CAN3_RX_PIN  (8)
#define CAN3_TX_PORT (GPIOA)
#define CAN3_TX_PIN  (15)

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

extern volatile uint32_t last_can_rx_time_ms;

void HardFault_Handler();

#endif // MAIN_H
