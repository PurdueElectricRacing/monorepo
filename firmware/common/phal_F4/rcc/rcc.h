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
#ifndef _PHAL_RCC_H_
#define _PHAL_RCC_H_

#include "common/phal_F4/phal_F4.h"
#include "common/phal_F4/rcc/pll.h"

#if !defined(HSE_VALUE) || HSE_VALUE == 0
#error "Define HSE_VALUE to the board HSE frequency in Hz for both CMSIS and PHAL"
#endif
#define HSE_CLOCK_RATE_HZ (HSE_VALUE)

#define HSI_CLOCK_RATE_HZ (16000000)
#define MCO_OUT_PIN       (8)

// RCC Constants
#if defined(STM32F423xx)

#define RCC_MAX_SYSCLK_TARGET_HZ      (100000000)

#else
#error "Please define a MCU arch"
#endif

typedef enum {
    PLL_SRC_HSI16,
    PLL_SRC_HSE
} PLLSrc_t;

typedef enum {
    MCO1_SRC_HSI = 0,
    MCO1_SRC_LSE = 1,
    MCO1_SRC_HSE = 2,
    MCO1_SRC_PLL = 3,

} MCO1Source_t;

typedef enum {
    MCO_DIV_NONE = 0,
    MCO_DIV_2    = 4,
    MCO_DIV_3    = 5,
    MCO_DIV_4    = 6,
    MCO_DIV_5    = 7

} MCODivisor_t;

typedef enum {
    CLOCK_SOURCE_HSI = 0,
    CLOCK_SOURCE_HSE = 1,
} ClockSrc_t;

typedef enum {
    RCC_ERROR_AHB_INIT    = 0,
    RCC_ERROR_APB1_INIT   = 1,
    RCC_ERROR_APB2_INIT   = 2,
    RCC_ERROR_HSI_INIT    = 3,
    RCC_ERROR_PLLSYS_INIT = 4,
    RCC_ERROR_PLLVCO_INIT = 5,
    RCC_ERROR_HSE_INIT    = 6,
} RCCErrors_t;

typedef struct {
    ClockSrc_t clock_source; /* Use HSE or not */
    bool use_pll; /* Use PLL or not */
    uint32_t system_clock_target_hz; /* System Core Clock rate */
    uint32_t ahb_clock_target_hz; /* AHB clock rate target */
    uint32_t apb1_clock_target_hz; /* APB1 clock rate target */
    uint32_t apb2_clock_target_hz; /* APB2 clock rate target */

    /* Only used for use_pll == true */
    PLLSrc_t pll_src; /* Input source for PLL VCO */
    uint32_t vco_output_rate_target_hz; /* VCO output target rate */
    uint32_t msi_output_rate_target_hz; /* Use if pll_src == MSI */
} ClockRateConfig_t;

/**
 * @brief Configure all AHB/APB/System clocks from the provided configuration.
 *
 * @param config Configuration to try to match
 * @return Binary encoded representation of which clocks were unsuccessfuly configured.
 *  return value of 0 means all clocks were sucessfully configured.
 */
uint8_t PHAL_configureClockRates(ClockRateConfig_t* config);

/**
 * @brief Configure PLL VCO Clock rate
 * The VCO clock is the input clock for the different PLL outputs.
 * Each PLL output will divide the VCO clock to get its output.
 *
 * @param pll_source PLL input source (HSI16 or HSE)
 * @param vco_output_rate_target_hz Target rate for PLL output
 * @return true
 * @return false Invalid source, out-of-range rate, or no exact M/N solution;
 *               these errors leave registers unchanged. Hardware transitions
 *               still require the RCC-03 timeout/safe-switch work.
 */
bool PHAL_configurePLLVCO(PLLSrc_t pll_source, uint32_t vco_output_rate_target_hz);

/**
 * @brief Configure PLL CLK as the System Clock at the desired target frequency.
 * SHOULD BE DONE BEFORE ANY OF THE AHB OR APB CLOCKS ARE CHANGED
 *
 * @param system_clock_target_hz
 * @return true Successfully configured PLL clock as system clock
 * @return false
 */
bool PHAL_configurePLLSystemClock(uint32_t system_clock_target_hz);

/**
 * @brief Configure HSI CLK as the System Clock.
 * SHOULD BE DONE BEFORE ANY OF THE AHB OR APB CLOCKS ARE CHANGED
 *
 * @return true Successfully configured HSI clock as system clock
 * @return false
 */
bool PHAL_configureHSISystemClock();

/**
 * @brief Configure HSE CLK as the System Clock.
 * SHOULD BE DONE BEFORE ANY OF THE AHB OR APB CLOCKS ARE CHANGED
 *
 * @return true Successfully configured HSE clock as system clock
 * @return false
 */
bool PHAL_configureHSESystemClock();

/**
 * @brief Configure AHB Clock rate by modifying the AHB prescaler value.
 *
 * @param ahb_clock_target_hz
 * @return true Successfully configured AHB clock rate to @param ahb_clock_target_hz
 * @return false
 */
bool PHAL_configureAHBClock(uint32_t ahb_clock_target_hz);

/**
 * @brief Configure APB1 Clock rate by modifying the APB1 prescaler value.
 *
 * @param apb1_clock_target_hz
 * @return true Successfully configured AHB clock rate to @param apb1_clock_target_hz
 * @return false
 */
bool PHAL_configureAPB1Clock(uint32_t apb1_clock_target_hz);

/**
 * @brief Configure APB1 Clock rate by modifying the APB2 prescaler value.
 *
 * @param apb2_clock_target_hz
 * @return true Successfully configured AHB clock rate to @param apb2_clock_target_hz
 * @return false
 */
bool PHAL_configureAPB2Clock(uint32_t apb2_clock_target_hz);

/**
 * @brief Trim the HSI clock
 * 0 <= trim_val <= 31
 * 17 = increase speed by ~0.2%
 * 15 = decrease speed by ~0.2%
 */
void PHAL_trimHSI(uint8_t trim_val);

/**
 * @brief Enable the Clock Out Pin 1
 *  It is highly recommended to change this
 *  only after reset before enabling the
 *  external oscillators and the PLL.
 * 
 *  This function should be called after GPIO initialization using 
 *   GPIO_INIT_MCO1_PA8
 *
 * @param bank GPIO bank
 * @param pin GPIO bank
 * @param source Clock source to send to the pin
 * @param division Division of clock source sent to the pin
 * @return true Successfully enabled the clock out
 * @return false
 */

bool PHAL_enableMCO1(GPIO_TypeDef* bank, uint8_t pin, MCO1Source_t source, MCODivisor_t division);

#endif // _PHAL_PLL_H_
