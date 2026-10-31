/**
 * @file watchdog.c
 * @brief Basic watchdog implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "watchdog.h"

#if defined(STM32F423xx)
#include "stm32f423xx.h"
#elif defined(STM32G474xx)
#include "stm32g474xx.h"
#else
#error "unsupported watchdog arch"
#endif

volatile uint32_t saved_CSR = 0;

void WDG_init(void) {
    // save and reset CSR
    saved_CSR = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;

    // pause the watchdog during debugging
#if defined(STM32F423xx)
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
#elif defined(STM32G474xx)
    DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;
#endif

    // setup watchdog
    IWDG->KR = 0x5555;
    IWDG->PR = (IWDG_PR_PR_1 | IWDG_PR_PR_0);
    IWDG->RLR = WATCHDOG_TIMEOUT_MS;

    IWDG->KR = 0xAAAA; // refresh
    IWDG->KR = 0xCCCC; // start
}

void WDG_pet(void) {
    IWDG->KR = 0xAAAA;
}

uint32_t WDG_get_CSR(void) {
    return saved_CSR;
}

bool was_reset_by_WDG(void) {
    return (saved_CSR & RCC_CSR_IWDGRSTF) != 0;
}