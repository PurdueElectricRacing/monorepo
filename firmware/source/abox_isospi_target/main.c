/* Standalone abox isoSPI traffic generator. */
#include "adbms.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#define SPI1_CS_PORT GPIOB
#define SPI1_CS_PIN 0

static PHAL_DMA_Handle_t spi1_rx_dma = {
    .wiring = &SPI1_RX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

static PHAL_DMA_Handle_t spi1_tx_dma = {
    .wiring = &SPI1_TX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

SPI_InitConfig_t bms_spi_config = {
    .data_len      = 8,
    .nss_sw        = false, // BMS drive CS pin manually to ensure correct timing
    .nss_gpio_port = SPI1_CS_PORT,
    .nss_gpio_pin  = SPI1_CS_PIN,
    .rx_dma        = &spi1_rx_dma,
    .tx_dma        = &spi1_tx_dma,
    .periph        = SPI1,
    .cpol = SPI_CPOL_IDLE_LOW,
    .cpha = SPI_CPHA_FIRST_EDGE,
    .data_rate     = 500'000, // 500 kHz SPI clock for ADBMS6380
};


static PHAL_GPIO_InitConfig_t pins[] = {
    PHAL_GPIO_INIT_OUTPUT(GPIOB, 5, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(GPIOB, 9, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(SPI1_CS_PORT, SPI1_CS_PIN, GPIO_OUTPUT_ULTRA_SPEED),
    PHAL_PIN_DEFS_SPI1_SCK_PA5,
    PHAL_PIN_DEFS_SPI1_MISO_PA6,
    PHAL_PIN_DEFS_SPI1_MOSI_PA7,
};
adbms_bms_t g_bms;
uint8_t g_bms_tx_buf[ADBMS_SPI_TX_BUFFER_SIZE];
void HardFault_Handler(void) {
    __disable_irq();
    GPIOB->BSRR = 1U << 9;
    for (;;) { __NOP(); }
}
static void heartbeat(void) { PHAL_GPIO_toggle(GPIOB, 5); }
static void bms_task(void) { adbms_periodic(&g_bms, 3.2f, 0.05f); }
RTOS_DEFINE_TASK(heartbeat, 100, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(bms_task, 200, TASK_PRIORITY_NORMAL, STACK_2048);
int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);
    if (!PHAL_GPIO_init(pins, countof(pins))) { HardFault_Handler(); }
    adbms6380_set_cs_high(&bms_spi_config);
    if (!PHAL_SPI_init(&bms_spi_config)) { HardFault_Handler(); }
    adbms_init(&g_bms, &bms_spi_config, g_bms_tx_buf);
    RTOS_START_TASK(heartbeat);
    RTOS_START_TASK(bms_task);
    vTaskStartScheduler();
    for (;;) { __NOP(); }
}
