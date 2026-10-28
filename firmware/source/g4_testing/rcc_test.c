#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_RCC)

/**
 * @file rcc_test.c
 * @brief Self-checking bench test for the G4 RCC driver.
 *
 * Runs on a NUCLEO-G474RE (HSI modes only, the Nucleo has no HSE).
 * Steps through 16 -> 170 -> 170 -> 16 -> 170 MHz, which covers every
 * transition PHAL_RCC_init must survive (including re-running it while the
 * PLL already drives SYSCLK, as happens after the bootloader hands off).
 * After each step the hardware registers are checked against independently
 * written expected values, not the driver's own constants.
 *
 * Result: green LED = all steps passed, red LED = a check failed. On failure
 * read rcc_failed_step / rcc_failed_check / rcc_failed_expected /
 * rcc_failed_actual with the debugger.
 */

#include <stdint.h>

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

void HardFault_Handler(void);

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

typedef enum {
    RCC_CHECK_SYSCLK_SOURCE,
    RCC_CHECK_PLL_SOURCE,
    RCC_CHECK_PLL_M,
    RCC_CHECK_PLL_N,
    RCC_CHECK_PLL_R,
    RCC_CHECK_PLL_R_ENABLED,
    RCC_CHECK_FLASH_LATENCY,
    RCC_CHECK_BOOST_MODE,
    RCC_CHECK_BUS_PRESCALERS,
    RCC_CHECK_SYSTEM_CORE_CLOCK,
    RCC_CHECK_GETTER_SYSTEM,
    RCC_CHECK_GETTER_AHB,
    RCC_CHECK_GETTER_APB1,
    RCC_CHECK_GETTER_APB2,
    NUM_RCC_CHECKS
} rcc_check_id_t;

typedef struct {
    PHAL_RCC_Mode_t mode;
    uint32_t expected_clock_hz;
    bool uses_pll;
} rcc_test_step_t;

static constexpr uint32_t CLOCK_16MHZ  = 16'000'000U;
static constexpr uint32_t CLOCK_170MHZ = 170'000'000U;

static const rcc_test_step_t RCC_TEST_STEPS[] = {
    {PHAL_RCC_HSI_16MHZ, CLOCK_16MHZ, false},
    {PHAL_RCC_HSI_170MHZ, CLOCK_170MHZ, true},
    {PHAL_RCC_HSI_170MHZ, CLOCK_170MHZ, true}, // re-init while already on the PLL
    {PHAL_RCC_HSI_16MHZ, CLOCK_16MHZ, false},  // step down from boost/4 wait states
    {PHAL_RCC_HSI_170MHZ, CLOCK_170MHZ, true},
};

// Expected PLL register field encodings for 16 MHz / 4 * 85 / 2 = 170 MHz
static constexpr uint32_t EXPECTED_PLLM_FIELD = 4U - 1U;       // encoded as divisor - 1
static constexpr uint32_t EXPECTED_PLLN_FIELD = 85U;           // raw multiplier
static constexpr uint32_t EXPECTED_PLLR_FIELD = (2U / 2U) - 1U; // encoded as divisor / 2 - 1

static constexpr uint32_t EXPECTED_LATENCY_16MHZ  = 0U;
static constexpr uint32_t EXPECTED_LATENCY_170MHZ = 4U;

static constexpr uint32_t NO_FAILURE = 0xFFFF'FFFFU;

volatile uint32_t rcc_failed_step     = NO_FAILURE;
volatile uint32_t rcc_failed_check    = NUM_RCC_CHECKS;
volatile uint32_t rcc_failed_expected = 0U;
volatile uint32_t rcc_failed_actual   = 0U;

/// Record the first mismatch; returns whether the values matched
static bool rcc_expect(uint32_t step, rcc_check_id_t check, uint32_t expected, uint32_t actual) {
    if (expected == actual) {
        return true;
    }
    if (rcc_failed_step == NO_FAILURE) {
        rcc_failed_step     = step;
        rcc_failed_check    = check;
        rcc_failed_expected = expected;
        rcc_failed_actual   = actual;
    }
    return false;
}

static uint32_t rcc_field(uint32_t reg, uint32_t mask, uint32_t pos) {
    return (reg & mask) >> pos;
}

static bool rcc_check_pll(uint32_t step) {
    const uint32_t pllcfgr = RCC->PLLCFGR;
    bool ok = rcc_expect(step, RCC_CHECK_PLL_SOURCE, RCC_PLLCFGR_PLLSRC_HSI, pllcfgr & RCC_PLLCFGR_PLLSRC_Msk);
    ok &= rcc_expect(step, RCC_CHECK_PLL_M, EXPECTED_PLLM_FIELD,
                     rcc_field(pllcfgr, RCC_PLLCFGR_PLLM_Msk, RCC_PLLCFGR_PLLM_Pos));
    ok &= rcc_expect(step, RCC_CHECK_PLL_N, EXPECTED_PLLN_FIELD,
                     rcc_field(pllcfgr, RCC_PLLCFGR_PLLN_Msk, RCC_PLLCFGR_PLLN_Pos));
    ok &= rcc_expect(step, RCC_CHECK_PLL_R, EXPECTED_PLLR_FIELD,
                     rcc_field(pllcfgr, RCC_PLLCFGR_PLLR_Msk, RCC_PLLCFGR_PLLR_Pos));
    ok &= rcc_expect(step, RCC_CHECK_PLL_R_ENABLED, RCC_PLLCFGR_PLLREN, pllcfgr & RCC_PLLCFGR_PLLREN);
    // R1MODE cleared = Range 1 boost, required above 150 MHz
    ok &= rcc_expect(step, RCC_CHECK_BOOST_MODE, 0U, PWR->CR5 & PWR_CR5_R1MODE);
    return ok;
}

static bool rcc_check_step(uint32_t step, const rcc_test_step_t *expected) {
    const uint32_t cfgr   = RCC->CFGR;
    const uint32_t hz     = expected->expected_clock_hz;
    const uint32_t sws    = expected->uses_pll ? RCC_CFGR_SWS_PLL : RCC_CFGR_SWS_HSI;
    const uint32_t latency = expected->uses_pll ? EXPECTED_LATENCY_170MHZ : EXPECTED_LATENCY_16MHZ;
    const uint32_t div1   = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;

    bool ok = rcc_expect(step, RCC_CHECK_SYSCLK_SOURCE, sws, cfgr & RCC_CFGR_SWS_Msk);
    if (expected->uses_pll) {
        ok &= rcc_check_pll(step);
    }
    ok &= rcc_expect(step, RCC_CHECK_FLASH_LATENCY, latency,
                     rcc_field(FLASH->ACR, FLASH_ACR_LATENCY_Msk, FLASH_ACR_LATENCY_Pos));
    ok &= rcc_expect(step, RCC_CHECK_BUS_PRESCALERS, div1,
                     cfgr & (RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk));
    ok &= rcc_expect(step, RCC_CHECK_SYSTEM_CORE_CLOCK, hz, SystemCoreClock);
    ok &= rcc_expect(step, RCC_CHECK_GETTER_SYSTEM, hz, PHAL_RCC_getSystemClockHz());
    ok &= rcc_expect(step, RCC_CHECK_GETTER_AHB, hz, PHAL_RCC_getAHBClockHz());
    ok &= rcc_expect(step, RCC_CHECK_GETTER_APB1, hz, PHAL_RCC_getAPB1ClockHz());
    ok &= rcc_expect(step, RCC_CHECK_GETTER_APB2, hz, PHAL_RCC_getAPB2ClockHz());
    return ok;
}

int main(void) {
    bool all_passed = true;
    for (uint32_t step = 0U; step < countof(RCC_TEST_STEPS); step++) {
        PHAL_RCC_init(RCC_TEST_STEPS[step].mode);
        all_passed &= rcc_check_step(step, &RCC_TEST_STEPS[step]);
    }

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, all_passed);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, !all_passed);

    while (1) {
        __asm__("nop");
    }
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_RCC
