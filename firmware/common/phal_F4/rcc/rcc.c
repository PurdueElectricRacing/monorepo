/**
 * @file rcc.h
 * @author Chris McGalliard (cmcgalli@purdue.edu) - Port of L4 RCC by Adam Busch (busch8@purdue.edu)
 * @brief RCC Configuration Driver for STM32F4 Devices
 * @version 0.1
 * @date 2023-08-16
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "common/phal_F4/rcc/rcc.h"

/* Globals for Clock Rates */
uint32_t APB1ClockRateHz;
uint32_t APB2ClockRateHz;
uint32_t AHBClockRateHz;
uint32_t PLLClockRateHz;

uint8_t PHAL_configureClockRates(ClockRateConfig_t* config) {
    // Nonzero Bit Encoded Error Code Indicates Error
    uint8_t ret_code = 0;

    /* Configure System Clock */
    if (config->clock_source == CLOCK_SOURCE_HSE) {
        ret_code |= (!PHAL_configureHSESystemClock()) << RCC_ERROR_HSE_INIT;
    } else {
        ret_code |= (!PHAL_configureHSISystemClock()) << RCC_ERROR_HSI_INIT;
    }

    /* Configure PLL */
    if (config->use_pll) {
        ret_code |= (!PHAL_configurePLLVCO(config->pll_src, config->vco_output_rate_target_hz)) << RCC_ERROR_PLLVCO_INIT;
        ret_code |= (!PHAL_configurePLLSystemClock(config->system_clock_target_hz)) << RCC_ERROR_PLLSYS_INIT;
    }
    /* No PLL */
    else {
        if (config->clock_source == CLOCK_SOURCE_HSE) {
            config->system_clock_target_hz = HSE_CLOCK_RATE_HZ;
        } else {
            config->system_clock_target_hz = HSI_CLOCK_RATE_HZ;
        }
    }

    ret_code |= (!PHAL_configureAHBClock(config->ahb_clock_target_hz)) << RCC_ERROR_AHB_INIT; // Configure AHB Clock
    ret_code |= (!PHAL_configureAPB1Clock(config->apb1_clock_target_hz)) << RCC_ERROR_APB1_INIT; // Configure APB1 Clock
    ret_code |= (!PHAL_configureAPB2Clock(config->apb2_clock_target_hz)) << RCC_ERROR_APB2_INIT; // Configure APB2 Clock

    return ret_code;
}

bool PHAL_configurePLLVCO(PLLSrc_t pll_source, uint32_t vco_output_rate_target_hz) {
    // Ensure range for PLL output is 100Mhz <= PLL_CLK <= 432Mhz
    vco_output_rate_target_hz = vco_output_rate_target_hz > RCC_MAX_VCO_RATE_HZ ? RCC_MAX_VCO_RATE_HZ : vco_output_rate_target_hz;
    vco_output_rate_target_hz = vco_output_rate_target_hz < RCC_MIN_VCO_RATE_HZ ? RCC_MIN_VCO_RATE_HZ : vco_output_rate_target_hz;

    // Turn off and wait for PLL to disable
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY))
        ; // Wait for PLL to turn off
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLSRC_Msk | RCC_PLLCFGR_PLLN_Msk | RCC_PLLCFGR_PLLM_Msk); // Clear any PLL source, N, and M configuration

    // Select the PLL source and determine a desired input clock rate
    uint32_t pll_input_f_hz;
    switch (pll_source) {
        // 16 MHz HSI
        case PLL_SRC_HSI16:
            RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI; // Select HSI source
            while (!(RCC->CR & RCC_CR_HSIRDY)) // Wait for HSI to enable
                ;
            pll_input_f_hz = HSI_CLOCK_RATE_HZ; // Set local for PLL input clock rate
            break;
        case PLL_SRC_HSE:
            RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;
            while (!(RCC->CR & RCC_CR_HSERDY))
                ;
            pll_input_f_hz = HSE_CLOCK_RATE_HZ;
            break;
        default:
            return false; // Invalid PLL source
    }

    /* Search for a possible PLL configuration */
    uint8_t pll_input_divisor     = RCC_MIN_PLL_INPUT_DIVISOR; // PLLM
    uint8_t pll_output_multiplier = RCC_MIN_PLL_OUTPUT_MULTIPLIER; // PLLN
    bool valid_rate               = false;
    for (; pll_input_divisor <= RCC_MAX_PLL_INPUT_DIVISOR; pll_input_divisor++) // PLLM must be 2 to 63 (Pg. 227)
    {
        // VCO input frequency = PLL input clock frequency / PLLM with 2 <= PLLM <= 63
        uint32_t pll_vco_in_rate = pll_input_f_hz / pll_input_divisor;
        if (pll_vco_in_rate < 1000000 || pll_vco_in_rate > 2000000) // VCO input rate must be 1MHz to 2MHz (Pg. 227 PLLM)
        {
            continue;
        }

        // VCO output frequency = VCO input * PLLN
        for (; pll_output_multiplier <= RCC_MAX_PLL_OUTPUT_MULTIPLIER; pll_output_multiplier++) // PLLN must be 50 to 432 (Pg. 227)
        {
            if ((pll_input_f_hz / pll_input_divisor) * pll_output_multiplier == vco_output_rate_target_hz) {
                valid_rate = true;
                break;
            }
        }
        if (valid_rate)
            break;
    }

    if (!valid_rate)
        return false; // Unable to find a valid clock rate!

    RCC->PLLCFGR |= ((pll_input_divisor) << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk; // Set PLLM
    RCC->PLLCFGR |= ((pll_output_multiplier) << RCC_PLLCFGR_PLLN_Pos) & RCC_PLLCFGR_PLLN_Msk; // Set PLLN

    // Update global variable used to reference the PLL
    PLLClockRateHz = ((pll_input_f_hz / pll_input_divisor) * pll_output_multiplier);

    SystemCoreClockUpdate(); // Must be called each time the core clock HCLK changes
    return true;
}

bool PHAL_configurePLLSystemClock(uint32_t system_clock_target_hz) {
    // F423 SYSCLK is limited to 100 MHz.
    if (system_clock_target_hz > RCC_MAX_SYSCLK_TARGET_HZ) {
        system_clock_target_hz = RCC_MAX_SYSCLK_TARGET_HZ;
    }

    // Valid number for PLLP divisor are 2,4,6,8 (2 bit encoded)
    uint8_t pll_p_divisor = PLLClockRateHz / system_clock_target_hz;
    if (pll_p_divisor == 0 || pll_p_divisor % 2 != 0 || pll_p_divisor > 8) {
        return false;
    }

    uint8_t pll_q_divisor = PLLClockRateHz / 48000000;
    // SDIO's existing dividers require an exact 48 MHz kernel clock.
    if (PLLClockRateHz % 48'000'000U != 0 || pll_q_divisor <= 1 || pll_q_divisor > 15) {
        return false;
    }

    // Set the PLLP and PLLQ divisors
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLP_Msk | RCC_PLLCFGR_PLLQ_Msk);
    RCC->PLLCFGR |= (((pll_p_divisor / 2) - 1) << RCC_PLLCFGR_PLLP_Pos) & RCC_PLLCFGR_PLLP_Msk; // Divisor value to PLLP bits (Pg. 227)
    RCC->PLLCFGR |= (pll_q_divisor << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk;

    // F423: CK48MSEL=0 selects main PLLQ; SDIOSEL=0 selects that CK48 clock.
    RCC->DCKCFGR2 &= ~(RCC_DCKCFGR2_CK48MSEL | RCC_DCKCFGR2_SDIOSEL);

    // PWR_CR.VOS is writable with PLL off; the caller's VCO setup leaves it off.
    // Scale 2 (reset) supports up to 84 MHz; F423 needs Scale 1 above that.
    if (system_clock_target_hz > 84'000'000) {
        RCC->APB1ENR |= RCC_APB1ENR_PWREN;
        (void)RCC->APB1ENR; // Peripheral-clock enable settling (ES0372 section 2.2.7).
        PWR->CR = (PWR->CR & ~PWR_CR_VOS_Msk) | PWR_CR_VOS;
    }

    __DSB(); // Complete PLL/VOS configuration before enabling PLL.
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        // Wait for PLL to turn on.
    }
    // VOS takes effect only with PLL on, while SYSCLK is still HSI/HSE.
    if (system_clock_target_hz > 84'000'000) {
        while (!(PWR->CSR & PWR_CSR_VOSRDY)) {
            // Wait for Scale 1 before selecting the faster system clock.
        }
    }

    // RM0430 table 6: 25 MHz per wait state at VDD=2.7–3.6 V (3V3 board).
    // SYSCLK bounds HCLK, including its final prescaler. Keep a higher existing
    // latency rather than reducing it before a possible clock decrease.
    const uint32_t latency = (system_clock_target_hz - 1U) / 25'000'000U;
    if ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) < (latency << FLASH_ACR_LATENCY_Pos)) {
        FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | (latency << FLASH_ACR_LATENCY_Pos);
        while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (latency << FLASH_ACR_LATENCY_Pos)) {
            // Confirm the new latency before increasing HCLK (RM0430 section 3.4.1).
        }
    }

    // F423 APB1 is limited to 50 MHz; SYSCLK may reach 100 MHz before the
    // caller applies its final bus dividers. Preserve any existing /2 or slower.
    if ((RCC->CFGR & RCC_CFGR_PPRE1_Msk) < RCC_CFGR_PPRE1_DIV2) {
        RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE1_Msk) | RCC_CFGR_PPRE1_DIV2;
    }

    __DSB(); // Wait for explicit memory accesses to finish
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL; // Set system clock switch register to PLL
    while ((RCC->CFGR & RCC_CFGR_SWS_PLL) != RCC_CFGR_SWS_PLL) // Wait for PLL to be the new system clock
        ;
    __DSB(); // Wait for explicit memory accesses to finish

    SystemCoreClockUpdate(); // Must be called each time the core clock HCLK changes
    return true;
}

bool PHAL_configureHSISystemClock() {
    // Turn on and wait for HSI to enable
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
        ;

    //Flash latency adjustment, see ST RM0430
    uint32_t flash_acr_temp = FLASH->ACR;
    flash_acr_temp &= ~(FLASH_ACR_LATENCY_Msk);
    flash_acr_temp |= FLASH_ACR_LATENCY_0WS << FLASH_ACR_LATENCY_Pos;
    FLASH->ACR = flash_acr_temp;

    __DSB(); // Wait for explicit memory accesses to finish
    RCC->CFGR |= RCC_CFGR_SW_HSI; // Set system clock switch register to HSI
    while ((RCC->CFGR & RCC_CFGR_SWS_HSI) != RCC_CFGR_SWS_HSI) // Wait until the system clock switch register indicates that HSI is selected
        ;
    __DSB(); // Wait for explicit memory accesses to finish

    SystemCoreClockUpdate(); // Must be called each time the core clock HCLK changes
    return true; // Return true upon completion
}

bool PHAL_configureHSESystemClock() {
    /* Turn on and wait for HSE to enable */
    RCC->CR |= (RCC_CR_HSEON | RCC_CR_HSEBYP);
    while (!(RCC->CR & RCC_CR_HSERDY))
        ;

    //Flash latency adjustment, see ST RM0430
    uint32_t flash_acr_temp = FLASH->ACR;
    flash_acr_temp &= ~(FLASH_ACR_LATENCY_Msk);
    flash_acr_temp |= FLASH_ACR_LATENCY_0WS << FLASH_ACR_LATENCY_Pos;
    FLASH->ACR = flash_acr_temp;

    __DSB(); // Wait for explicit memory accesses to finish
    RCC->CFGR |= RCC_CFGR_SW_HSE; // Set system clock switch register to HSE
    while ((RCC->CFGR & RCC_CFGR_SWS_HSE) != RCC_CFGR_SWS_HSE) // Wait until the system clock switch register indicates that HSE is selected
        ;
    __DSB(); // Wait for explicit memory accesses to finish

    /* Turn off HSI */
    __DSB();
    RCC->CR &= ~(RCC_CR_HSION);
    while ((RCC->CR & RCC_CR_HSION))
        ;
    __DSB();

    SystemCoreClockUpdate(); // Must be called each time the core clock HCLK changes

    return true;
}

bool PHAL_configureAHBClock(uint32_t ahb_clock_target_hz) {
    // Map a required prescaler to achieve the target AHB clock speed from the SystemCoreClock
    // F423 HCLK is limited to 100 MHz at voltage scale 1; see RM0430.
    uint32_t desired_psc = SystemCoreClock / ahb_clock_target_hz;
    uint32_t sys_clk_div;

    // CPU Clock Prescalar (1,2,4,8,16,64,128,256,512 allowed)
    switch (desired_psc) // ST RM0430
    {
        case 1:
            sys_clk_div = RCC_CFGR_HPRE_DIV1;
            break;
        case 2:
            sys_clk_div = RCC_CFGR_HPRE_DIV2;
            break;
        case 4:
            sys_clk_div = RCC_CFGR_HPRE_DIV4;
            break;
        case 8:
            sys_clk_div = RCC_CFGR_HPRE_DIV8;
            break;
        case 16:
            sys_clk_div = RCC_CFGR_HPRE_DIV16;
            break;
        case 64:
            sys_clk_div = RCC_CFGR_HPRE_DIV64;
            break;
        case 128:
            sys_clk_div = RCC_CFGR_HPRE_DIV128;
            break;
        case 256:
            sys_clk_div = RCC_CFGR_HPRE_DIV256;
            break;
        case 512:
            sys_clk_div = RCC_CFGR_HPRE_DIV512;
            break;
        default:
            return false; // Return if invalid prescalar value
    }

    // Modify the RCC->CFGR HPRE bits to select prescaler
    uint32_t rcc_cfgr_temp = RCC->CFGR;
    rcc_cfgr_temp &= ~(RCC_CFGR_HPRE_Msk);
    rcc_cfgr_temp |= (sys_clk_div);
    RCC->CFGR = rcc_cfgr_temp;

    SystemCoreClockUpdate(); // HPRE changes HCLK, which CMSIS/FreeRTOS report as the core clock.
    AHBClockRateHz = SystemCoreClock;
    return true;
}

// Low speed peripheral bus (F423 maximum 50 MHz)
bool PHAL_configureAPB1Clock(uint32_t apb1_clock_target_hz) {
    // Map a required prescaler to achieve the target APB1 clock speed from AHB
    uint32_t desired_psc = AHBClockRateHz / apb1_clock_target_hz;
    uint32_t ahb_clk_div;

    // APB1 Prescaler (1,2,4,8,16 allowed)
    switch (desired_psc) // ST RM0430
    {
        case 1:
            ahb_clk_div = RCC_CFGR_PPRE1_DIV1;
            break;
        case 2:
            ahb_clk_div = RCC_CFGR_PPRE1_DIV2;
            break;
        case 4:
            ahb_clk_div = RCC_CFGR_PPRE1_DIV4;
            break;
        case 8:
            ahb_clk_div = RCC_CFGR_PPRE1_DIV8;
            break;
        case 16:
            ahb_clk_div = RCC_CFGR_PPRE1_DIV16;
            break;
        default:
            return false; // Return if invalid prescaler value
    }

    // Modify the RCC->CFGR PPRE1 bits to select prescaler
    uint32_t rcc_cfgr_temp = RCC->CFGR;
    rcc_cfgr_temp &= ~(RCC_CFGR_PPRE1_Msk);
    rcc_cfgr_temp |= (ahb_clk_div);
    RCC->CFGR = rcc_cfgr_temp;

    APB1ClockRateHz = AHBClockRateHz
        >> APBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE1_Msk) >> RCC_CFGR_PPRE1_Pos];
    return true;
}

// High speed peripheral bus (F423 maximum 100 MHz)
bool PHAL_configureAPB2Clock(uint32_t apb2_clock_target_hz) {
    // Map a required prescaler to achieve the target APB2 clock speed from AHB
    uint32_t desired_psc = AHBClockRateHz / apb2_clock_target_hz;
    uint32_t ahb_clk_div;

    // APB1 Prescaler (1,2,4,8,16 allowed)
    switch (desired_psc) // ST RM0430
    {
        case 1:
            ahb_clk_div = RCC_CFGR_PPRE2_DIV1;
            break;
        case 2:
            ahb_clk_div = RCC_CFGR_PPRE2_DIV2;
            break;
        case 4:
            ahb_clk_div = RCC_CFGR_PPRE2_DIV4;
            break;
        case 8:
            ahb_clk_div = RCC_CFGR_PPRE2_DIV8;
            break;
        case 16:
            ahb_clk_div = RCC_CFGR_PPRE2_DIV16;
            break;
        default:
            return false; // Invalid prescaler value
    }

    // Modify the RCC->CFGR PPRE2 bits to select prescaler
    uint32_t rcc_cfgr_temp = RCC->CFGR;
    rcc_cfgr_temp &= ~(RCC_CFGR_PPRE2_Msk);
    rcc_cfgr_temp |= ahb_clk_div;
    RCC->CFGR = rcc_cfgr_temp;

    APB2ClockRateHz = AHBClockRateHz
        >> APBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE2_Msk) >> RCC_CFGR_PPRE2_Pos];
    return true;
}

void PHAL_trimHSI(uint8_t trim_val) {
    if (trim_val > 31)
        return;
    uint32_t reg = RCC->CR;
    reg &= ~(RCC_CR_HSITRIM);
    reg |= trim_val << RCC_CR_HSITRIM_Pos;
    RCC->CR = reg;
}

bool PHAL_enableMCO1(GPIO_TypeDef* bank, uint8_t pin, MCO1Source_t source, MCODivisor_t division) {
    if ((bank == GPIOA) && (pin == MCO_OUT_PIN)) {
        /* Clear Previous MCO1 Source */
        RCC->CFGR &= ~(RCC_CFGR_MCO1_Msk);

        /* Select MCO1 Source*/
        RCC->CFGR |= (source << RCC_CFGR_MCO1_Pos);

        /* Clear Previous Prescaler */
        RCC->CFGR &= ~(RCC_CFGR_MCO1PRE_Msk);

        /* Select MCO1 Prescaler */
        RCC->CFGR |= (division << RCC_CFGR_MCO1PRE_Pos);
    } else {
        return false;
    }

    return true;

} /* PHAL_enableMCO1() */
