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
        while (1) // PLLN must be 50 to 432 (Pg. 227)
        {
            if ((pll_input_f_hz / pll_input_divisor) * pll_output_multiplier == vco_output_rate_target_hz) {
                valid_rate = true;
                break;
            }
            pll_output_multiplier += 1;
        }
        if (valid_rate)
            break;
    }

    if (!valid_rate)
        return false; // Unable to find a valid clock rate!

    RCC->PLLCFGR |=
        ((uint32_t)pll_input_divisor << RCC_PLLCFGR_PLLM_Pos) & RCC_PLLCFGR_PLLM_Msk; // Set PLLM
    RCC->PLLCFGR |= ((uint32_t)pll_output_multiplier << RCC_PLLCFGR_PLLN_Pos)
        & RCC_PLLCFGR_PLLN_Msk; // Set PLLN

    // Update global variable used to reference the PLL
    PLLClockRateHz = ((pll_input_f_hz / pll_input_divisor) * pll_output_multiplier);

    SystemCoreClockUpdate(); // Must be called each time the core clock HCLK changes
    return true;
}

bool PHAL_configurePLLSystemClock(uint32_t system_clock_target_hz) {
    // Ensure sysetm clock target is valid (Must be under 168 MHz)
    if (system_clock_target_hz > RCC_MAX_SYSCLK_TARGET_HZ) {
        system_clock_target_hz = RCC_MAX_SYSCLK_TARGET_HZ;
    }

    // Valid number for PLLP divisor are 2,4,6,8 (2 bit encoded)
    uint32_t pll_p_divisor = PLLClockRateHz / system_clock_target_hz;
    if (pll_p_divisor == 0 || pll_p_divisor % 2 != 0 || pll_p_divisor > 8) {
        return false;
    }

    uint32_t pll_q_divisor = PLLClockRateHz / 48000000;
    if (pll_q_divisor <= 1 || pll_q_divisor > 15) {
        return false;
    }

    // Set the PLLP and PLLQ divisors
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLP_Msk | RCC_PLLCFGR_PLLQ_Msk);
    RCC->PLLCFGR |= (((pll_p_divisor / 2) - 1) << RCC_PLLCFGR_PLLP_Pos)
        & RCC_PLLCFGR_PLLP_Msk; // Divisor value to PLLP bits (Pg. 227)
    RCC->PLLCFGR |= (pll_q_divisor << RCC_PLLCFGR_PLLQ_Pos) & RCC_PLLCFGR_PLLQ_Msk;

    __DSB(); // Wait for explicit memory accesses to finish

    RCC->CR |= RCC_CR_PLLON; // Enable PLL
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ; // Wait for PLL to turn on
    __DSB();

    //Flash latency adjustment, see ST RM 0090 Pg. 80, ST RM 0431 Pg. 69
    uint32_t flash_acr_temp = FLASH->ACR;
    flash_acr_temp &= ~(FLASH_ACR_LATENCY_Msk);

    if (system_clock_target_hz >= 210000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_7WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 180000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_6WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 150000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_5WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 120000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_4WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 90000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_3WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 60000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_2WS << FLASH_ACR_LATENCY_Pos;
    else if (system_clock_target_hz >= 30000000)
        flash_acr_temp |= FLASH_ACR_LATENCY_1WS << FLASH_ACR_LATENCY_Pos;
    else
        flash_acr_temp |= FLASH_ACR_LATENCY_0WS << FLASH_ACR_LATENCY_Pos;
    FLASH->ACR = flash_acr_temp;

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

    //Flash latency adjustment, see ST RM 0090 Pg. 80
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

    //Flash latency adjustment, see ST RM 0090 Pg. 80
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
    // HCLK has a maximum frequency of 144MHz VOS = 0 and 168 MHz VOS = 1 RM 0090 pg. 80
    uint32_t desired_psc = SystemCoreClock / ahb_clock_target_hz;
    uint32_t sys_clk_div;

    // CPU Clock Prescalar (1,2,4,8,16,64,128,256,512 allowed)
    switch (desired_psc) // ST RM0090 pg. 230
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

    AHBClockRateHz = ahb_clock_target_hz; // Set global for AHB Clock Rate
    return true;
}

// Low speed peripheral bus (Max speed 42 MHz)
bool PHAL_configureAPB1Clock(uint32_t apb1_clock_target_hz) {
    // Map a required prescaler to achieve the target APB1 clock speed from AHB
    uint32_t desired_psc = AHBClockRateHz / apb1_clock_target_hz;
    uint32_t ahb_clk_div;

    // APB1 Prescaler (1,2,4,8,16 allowed)
    switch (desired_psc) // ST RM0090 pg. 229
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

    APB1ClockRateHz = apb1_clock_target_hz; // Set global for APB1
    return true;
}

// High speed peripheral bus (Max speed 84 MHz)
bool PHAL_configureAPB2Clock(uint32_t apb2_clock_target_hz) {
    // Map a required prescaler to achieve the target APB2 clock speed from AHB
    uint32_t desired_psc = AHBClockRateHz / apb2_clock_target_hz;
    uint32_t ahb_clk_div;

    // APB1 Prescaler (1,2,4,8,16 allowed)
    switch (desired_psc) // ST RM0090 pg. 229
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

    APB2ClockRateHz = apb2_clock_target_hz; // Set global for APB2
    return true;
}

void PHAL_trimHSI(uint8_t trim_val) {
    if (trim_val > 31)
        return;
    uint32_t reg = RCC->CR;
    reg &= ~(RCC_CR_HSITRIM);
    reg |= (uint32_t)trim_val << RCC_CR_HSITRIM_Pos;
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
