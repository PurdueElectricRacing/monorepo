/**
 * @file rcc_priv.c
 * @brief G4 RCC private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/rcc/rcc_priv.h"

// Boost mode has no ready flag to poll, so wait a short fixed interval
static constexpr uint32_t RCC_PRIV_BOOST_SETTLE_ITERATIONS = 100U;

// Each loop iteration takes at least one cycle, so at HCLK = 85 MHz this
// waits >= 2 us: double the 1 us RM0440 requires
static constexpr uint32_t RCC_PRIV_BOOST_TRANSITION_ITERATIONS = 170U;

void PHAL_RCC_priv_enableHSI(void) {
    // CR = Clock control register
    // - top-level enable/ready bits for each clock source
    // HSION = HSI16 (High Speed Internal, 16 MHz) Oscillator ON bit
    // - set to request the internal 16 MHz RC oscillator to start
    RCC->CR |= RCC_CR_HSION;

    // HSIRDY = HSI16 Ready flag
    // - hardware sets this once HSI16 has stabilized and is safe to use
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_disableHSI(void) {
    // Clear HSION (HSI16 Oscillator ON bit) to request HSI16 to stop
    // - only safe once nothing (SYSCLK, PLL source, etc) still depends on it
    RCC->CR &= ~RCC_CR_HSION;

    // Wait until HSI16 is confirmed stopped
    // HSIRDY = HSI16 Ready flag
    // - the actual hardware-confirmed status bit
    // - HSION only reflects the request and clears immediately on write
    while (RCC->CR & RCC_CR_HSIRDY) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_enableHSE(void) {
    // HSEON = HSE (High Speed External) Oscillator ON bit
    // - set to request the external oscillator/clock input to start
    // HSEBYP = HSE Bypass bit
    // - set because this board feeds HSE a clock signal directly rather
    //   than a crystal, so the internal oscillator amplifier is bypassed
    RCC->CR |= (RCC_CR_HSEON | RCC_CR_HSEBYP);

    // HSERDY = HSE Ready flag
    // - hardware sets this once HSE has stabilized and is safe to use
    while (!(RCC->CR & RCC_CR_HSERDY)) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_setFlashLatency(uint32_t latency_ws) {
    // ACR = Access control register (Flash)
    // - controls Flash read timing among other things
    uint32_t acr = FLASH->ACR;

    // LATENCY = Flash read Latency field
    // - number of extra wait states inserted per Flash read
    // - must be raised before HCLK increases past that latency's rated
    //   frequency, or Flash reads become unreliable
    acr &= ~FLASH_ACR_LATENCY_Msk;
    acr |= (latency_ws << FLASH_ACR_LATENCY_Pos);
    FLASH->ACR = acr;

    // RM0440 requires reading LATENCY back to confirm the new wait states
    // are in effect before the clock is raised
    while (((FLASH->ACR & FLASH_ACR_LATENCY_Msk) >> FLASH_ACR_LATENCY_Pos) != latency_ws) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_configurePLL(RCC_PRIV_PLLSource_t src) {
    // PLLON = PLL ON bit
    // - clear and wait for it to  turn off before touching PLLCFGR
    // - PLLM/PLLN/PLLR/PLLSRC are only writable while the PLL is disabled
    RCC->CR &= ~RCC_CR_PLLON;

    // PLLRDY = PLL Ready flag
    // - hardware clears this once the PLL has stopped
    while (RCC->CR & RCC_CR_PLLRDY) {
        __asm__("nop");
    }

    // PLLCFGR = PLL configuration register
    // PLLSRC = PLL Source field
    // - selects which oscillator (HSI16 or HSE) feeds the PLL
    // PLLM = PLL input division factor field
    // - divides the source oscillator down to the PLL's VCO input rate
    // PLLN = PLL VCO multiplication factor field
    // - multiplies the VCO input rate up to the VCO's internal output rate
    // PLLR = PLL R-output division factor field
    // - divides the VCO output down to produce the clock this driver
    //   feeds to SYSCLK
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLSRC_Msk | RCC_PLLCFGR_PLLM_Msk | RCC_PLLCFGR_PLLN_Msk | RCC_PLLCFGR_PLLR_Msk);

    // PLLM field is encoded as (divisor - 1)
    // PLLN is the raw multiplier with no bias
    // PLLR is encoded as (divisor / 2 - 1)
    RCC->PLLCFGR |= (uint32_t)src
        | ((RCC_PRIV_PLL_M - 1U) << RCC_PLLCFGR_PLLM_Pos)
        | (RCC_PRIV_PLL_N << RCC_PLLCFGR_PLLN_Pos)
        | (((RCC_PRIV_PLL_R / 2U) - 1U) << RCC_PLLCFGR_PLLR_Pos);

    // PLLREN = PLL R-output Enable bit
    // - without this the R divider is configured but its output is gated
    //   off; the P and Q outputs are left disabled since nothing in this
    //   driver uses them
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;

    // Re-enable the PLL now that source/M/N/R are configured
    RCC->CR |= RCC_CR_PLLON;

    // Wait for PLLRDY (PLL Ready flag) before returning: the PLL output
    // isn't valid/lockable until hardware confirms it here
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_enableBoostMode(void) {
    // APB1ENR1 = APB1 peripheral clock Enable Register 1
    // PWREN = Power interface clock Enable bit
    // - PWR peripheral (and PWR_CR5 below) isn't accessible until this is set
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    // Read back so the PWR clock is running before PWR_CR5 is written
    (void)RCC->APB1ENR1;

    // CFGR = Clock configuration register
    // HPRE = AHB Prescaler field
    // - HCLK (AHB clock) to be temporarily halved while boost mode is being enabled
    // - restored afterward
    //   - PHAL_RCC_priv_setBusPrescalersToDiv1() is called later once the switch
    //     to PLL is confirmed
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE_Msk) | RCC_CFGR_HPRE_DIV2;
    __DSB(); // let the prescaler change and take effect

    // PWR_CR5 = Power control register 5
    // R1MODE = Range 1 boost MODE bit
    // - clearing this bit is what enables boost mode, required
    //   for HCLK to exceed 150 MHz
    PWR->CR5 &= ~PWR_CR5_R1MODE;

    for (volatile uint32_t i = 0; i < RCC_PRIV_BOOST_SETTLE_ITERATIONS; i++) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_switchSysclkToHSI(void) {
    // SW = System clock Switch field
    // - selects which clock source SYSCLK should switch to
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_HSI;

    // SWS = System clock Switch Status field
    // - hardware-reported readback of which source currently driving SYSCLK
    // - wait until it confirms HSI16
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_HSI) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_switchSysclkToHSE(void) {
    // SW (System clock Switch field) = request HSE as SYSCLK's source
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_HSE;

    // Wait for SWS (System clock Switch Status field) to confirm HSE is
    // driving SYSCLK now
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_HSE) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_switchSysclkToPLL(void) {
    // SW (System clock Switch field) = request the PLL as SYSCLK's source
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_PLL;

    // Wait for SWS (System clock Switch Status field) to confirm the PLL
    // is driving SYSCLK now
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_waitBoostTransition(void) {
    for (volatile uint32_t i = 0; i < RCC_PRIV_BOOST_TRANSITION_ITERATIONS; i++) {
        __asm__("nop");
    }
}

void PHAL_RCC_priv_setBusPrescalersToDiv1(void) {
    uint32_t cfgr = RCC->CFGR;

    // HPRE = AHB Prescaler field (divides SYSCLK to get HCLK)
    // PPRE1 = APB1 Prescaler field (divides HCLK to get PCLK1)
    // PPRE2 = APB2 Prescaler field (divides HCLK to get PCLK2)
    // - all three set to divide-by-1: AHB/APB1/APB2 all run undivided at
    //   the full SYSCLK rate
    // - valid on G4 since APB1/APB2's maximum rate equals AHB's
    cfgr &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk);
    cfgr |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;
    RCC->CFGR = cfgr;
}