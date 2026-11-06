/**
 * @file main.c
 * @brief "DAQ" node source code
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Luke Oxley (lcoxley@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/generated/MCAN.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_F4/can/can.h"
#include "common/phal_F4/gpio/gpio.h"
#include "common/phal_F4/rcc/rcc.h"
#include "common/phal_F4/rtc/rtc.h"
#include "common/phal_F4/spi/spi.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "ethernet.h"
#include "rtc_sync.h"
#include "spmc.h"
#include "sd_card.h"

GPIOInitConfig_t gpio_config[] = {
    // LEDs
    GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(VCAN_LED_PORT, VCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(MCAN_LED_PORT, MCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(SCAN_LED_PORT, SCAN_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(SD_ACTIVITY_LED_PORT, SD_ACTIVITY_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(SD_ERROR_LED_PORT, SD_ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(SD_DETECT_LED_PORT, SD_DETECT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // W5500 ETH SPI1
    GPIO_INIT_AF(ETH_SCK_PORT, ETH_SCK_PIN, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_AF(ETH_MISO_PORT, ETH_MISO_PIN, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(ETH_MOSI_PORT, ETH_MOSI_PIN, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN),
    GPIO_INIT_OUTPUT(ETH_CS_PORT, ETH_CS_PIN, GPIO_OUTPUT_HIGH_SPEED),
    GPIO_INIT_OUTPUT_OPEN_DRAIN(ETH_RST_PORT, ETH_RST_PIN, GPIO_OUTPUT_LOW_SPEED),

    // SDIO
    GPIO_INIT_AF(SDIO_CLK_PORT, SDIO_CLK_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),      // CLK
    GPIO_INIT_AF(SDIO_CMD_PORT, SDIO_CMD_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),      // CMD
    GPIO_INIT_AF(SDIO_DAT0_PORT, SDIO_DAT0_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),    // DAT 0
    GPIO_INIT_AF(SDIO_DAT1_PORT, SDIO_DAT1_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),    // DAT 1
    GPIO_INIT_AF(SDIO_DAT2_PORT, SDIO_DAT2_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),    // DAT 2
    GPIO_INIT_AF(SDIO_DAT3_PORT, SDIO_DAT3_PIN, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP),    // DAT 3

    GPIO_INIT_INPUT(SD_CD_PORT, SD_CD_PIN, GPIO_INPUT_PULL_UP),
    GPIO_INIT_INPUT(LOG_ENABLE_PORT, LOG_ENABLE_PIN, GPIO_INPUT_PULL_DOWN), // ! pull down to fix floating input issue
    GPIO_INIT_INPUT(PWR_LOSS_PORT, PWR_LOSS_PIN, GPIO_INPUT_OPEN_DRAIN), // SPL EXTI

    // CAN1
    GPIO_INIT_AF(CAN1_RX_PORT, CAN1_RX_PIN, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(CAN1_TX_PORT, CAN1_TX_PIN, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    // CAN2
    GPIO_INIT_AF(CAN2_RX_PORT, CAN2_RX_PIN, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(CAN2_TX_PORT, CAN2_TX_PIN, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),

    // CAN3
    GPIO_INIT_AF(CAN3_RX_PORT, CAN3_RX_PIN, 11, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN),
    GPIO_INIT_AF(CAN3_TX_PORT, CAN3_TX_PIN, 11, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN),
};

/* CLOCK CONFIG */
extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;
extern uint32_t AHBClockRateHz;
extern uint32_t PLLClockRateHz;

#define TargetCoreClockrateHz 100'000'000
#define BLINK_PERIOD_MS 500
ClockRateConfig_t clock_config = {
    .clock_source              = CLOCK_SOURCE_HSE,
    .use_pll                   = true,
    .use_plli2s_for_48mhz      = true,
    .pll_src                   = PLL_SRC_HSE,
    .vco_output_rate_target_hz = 200'000'000,
    .system_clock_target_hz    = TargetCoreClockrateHz,
    .ahb_clock_target_hz       = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz      = (TargetCoreClockrateHz / 4),
    .apb2_clock_target_hz      = (TargetCoreClockrateHz / 4),
};

/* SPI CONFIG FOR ETHERNET MODULE */
dma_init_t spi_rx_dma_config    = SPI1_RXDMA_CONT_CONFIG(NULL, 2);
dma_init_t spi_tx_dma_config    = SPI1_TXDMA_CONT_CONFIG(NULL, 1);
SPI_InitConfig_t eth_spi_config = {
    .data_rate     = 12'500'000, // SPI1: 25 MHz PCLK2 / 2
    .data_len      = 8,
    .nss_sw        = false,
    .nss_gpio_port = ETH_CS_PORT,
    .nss_gpio_pin  = ETH_CS_PIN,
    .rx_dma_cfg    = &spi_rx_dma_config,
    .tx_dma_cfg    = &spi_tx_dma_config,
    .periph        = SPI1,
};

RTOS_DEFINE_MUTEX(spi1_lock);
volatile TaskHandle_t sd_task_handle = nullptr;

static void configure_interrupts(void);
void shutdown(void);

RTOS_DEFINE_TASK(sd_card_periodic, SD_FSM_PERIOD_MS, TASK_PRIORITY_HIGH, STACK_4096); // SD WRITE
RTOS_DEFINE_TASK(ethernet_periodic, 0, TASK_PRIORITY_NORMAL, STACK_4096); // BULLET COMMS 
RTOS_DEFINE_TASK(RTC_sync, 0, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main() {
    if (0 != PHAL_configureClockRates(&clock_config)) {
        HardFault_Handler();
    }
    WDG_init();
    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    if (!PHAL_SPI_init(&eth_spi_config)) {
        HardFault_Handler();
    }

    if (!PHAL_configureRTC(&fallback_timestamp, false)) {
        HardFault_Handler();
    }
    if (!PHAL_CAN_init(CAN1, SCAN_BAUD_RATE)) {
        HardFault_Handler();
    }
    if (!PHAL_CAN_init(CAN2, VCAN_BAUD_RATE)) {
        HardFault_Handler();
    }
    if (!PHAL_CAN_init(CAN3, MCAN_BAUD_RATE)) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(ETH_RST_PORT, ETH_RST_PIN, 1);

    RTC_sync_init();
    SPMC_init(&g_spmc); // also enables CAN interrupts
    configure_interrupts();

    RTOS_INIT_MUTEX(spi1_lock);

    RTOS_START_TASK(sd_card_periodic); // SD WRITE
    RTOS_START_TASK(ethernet_periodic); // BULLET COMMS
    RTOS_START_TASK(RTC_sync);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

static void configure_interrupts(void) {
    // Configure exti interupt for power loss pin (PE15)
    // Enable the SYSCFG clock for interrupts
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; 
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI1_PB; // Map PB1 to EXTI 1
    EXTI->IMR |= EXTI_IMR_MR1; // Unmask EXTI1
    EXTI->FTSR |= EXTI_FTSR_TR1; // Enable the falling edge trigger (active low reset)
    NVIC_SetPriority(EXTI1_IRQn, 15); // allow other interrupts to preempt this one (especially systick and dma)
    NVIC_EnableIRQ(EXTI1_IRQn);
}

// disables high power devices to give time for the SD card to flush
void power_loss_callback(void) {
    // hold W5500 in reset
    PHAL_GPIO_write(ETH_RST_PORT, ETH_RST_PIN, 0);

    // turn off LEDs
    PHAL_GPIO_write(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, 0);
    PHAL_GPIO_write(CONNECTION_LED_PORT, CONNECTION_LED_PIN, 0);
    PHAL_GPIO_write(ERROR_LED_PORT, ERROR_LED_PIN, 0);
    PHAL_GPIO_write(SD_ACTIVITY_LED_PORT, SD_ACTIVITY_LED_PIN, 0);
    PHAL_GPIO_write(SD_ERROR_LED_PORT, SD_ERROR_LED_PIN, 0);
    PHAL_GPIO_write(SD_DETECT_LED_PORT, SD_DETECT_LED_PIN, 0);
}

// Interrupt handler for power loss detection
// Note: this is set to lowest priority to allow preemption by other interrupts
void EXTI1_IRQHandler() {
    if (EXTI->PR & EXTI_PR_PR1) {
        EXTI->PR |= EXTI_PR_PR1; // Clear interrupt
    }
}

void HardFault_Handler() {
    ERROR_LED_PORT->BSRR = (1U << ERROR_LED_PIN);
    while (1) {
        __asm__("nop");
    }
}
