/* Bench-only seven-module ADBMS emulator behind a slave-mode isoSPI bridge. */
#include "protocol/emulator.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"

emulator_t g_emulator;
static bool selected;
static size_t tx_position;
static PHAL_GPIO_InitConfig_t pins[] = {
    PHAL_GPIO_INIT_OUTPUT(GPIOB, 5, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(GPIOB, 9, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_INPUT(GPIOB, 0, GPIO_INPUT_PULL_UP),
    PHAL_PIN_DEFS_SPI1_SCK_PA5,
    PHAL_PIN_DEFS_SPI1_MISO_PA6,
    PHAL_PIN_DEFS_SPI1_MOSI_PA7,
};
void HardFault_Handler(void) {
    __disable_irq();
    GPIOB->BSRR = 1U << 9;
    for (;;) { __NOP(); }
}
static void heartbeat(void) { PHAL_GPIO_toggle(GPIOB, 5); }
RTOS_DEFINE_TASK(heartbeat, 500, TASK_PRIORITY_LOW, STACK_512);

static void spi_reset(void) {
    /* Peripheral reset also empties TX FIFO/shift register after short frames. */
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    SPI1->CR1 = SPI_CR1_SSM | SPI_CR1_SSI; /* Slave, deselected, mode 0. */
    SPI1->CR2 = (7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
    tx_position = 0;
    /* Arm while CS is high so the falling edge only has to select the slave. */
    for (size_t i = 0; i < 4; ++i) { *(volatile uint8_t *)&SPI1->DR = 0; }
    SPI1->CR2 |= SPI_CR2_RXNEIE | SPI_CR2_ERRIE;
    SPI1->CR1 |= SPI_CR1_SPE;
}
static void receive_pending(void) {
    while ((SPI1->SR & SPI_SR_RXNE) != 0) {
        uint8_t rx = *(volatile uint8_t *)&SPI1->DR;
        (void)emulator_byte(&g_emulator, rx);
    }
}
static void fill_response(void) {
    if (!g_emulator.valid || !g_emulator.response) { return; }
    while (tx_position < EMU_RESPONSE_SIZE && (SPI1->SR & SPI_SR_TXE) != 0) {
        *(volatile uint8_t *)&SPI1->DR = g_emulator.response[tx_position++];
    }
    if (tx_position < EMU_RESPONSE_SIZE) { SPI1->CR2 |= SPI_CR2_TXEIE; }
    else { SPI1->CR2 &= ~SPI_CR2_TXEIE; }
}
void SPI1_IRQHandler(void) {
    if (!selected) { return; }
    if ((SPI1->SR & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) != 0) {
        ++g_emulator.stats.peripheral_errors;
        selected = false; /* Stop until next CS boundary. */
        emulator_end(&g_emulator);
        spi_reset();
        return;
    }
    receive_pending();
    fill_response();
}
void EXTI0_IRQHandler(void) {
    if ((EXTI->PR1 & 1U) == 0) { return; }
    EXTI->PR1 = 1U;
    if ((GPIOB->IDR & 1U) != 0) {
        /* Last RX byte can be pending when CS rises. Both IRQs have equal priority. */
        if (selected) { receive_pending(); }
        selected = false;
        emulator_end(&g_emulator);
        spi_reset();
    } else {
        selected = true;
        SPI1->CR1 &= ~SPI_CR1_SSI;
    }
}
int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);
    if (!PHAL_GPIO_init(pins, countof(pins))) { HardFault_Handler(); }
    emulator_init(&g_emulator);
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    spi_reset();
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI0_Msk) | SYSCFG_EXTICR1_EXTI0_PB;
    EXTI->RTSR1 |= 1U;
    EXTI->FTSR1 |= 1U;
    EXTI->PR1 = 1U;
    EXTI->IMR1 |= 1U;
    /* Above FreeRTOS syscall priority; neither interrupt calls the kernel. */
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_SetPriority(SPI1_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(SPI1_IRQn);
    RTOS_START_TASK(heartbeat);
    vTaskStartScheduler();
    for (;;) { __NOP(); }
}
