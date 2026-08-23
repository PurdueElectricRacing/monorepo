/**
 * @file rcc_priv.h
 * @brief G4 RCC private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef __PHAL_G4_RCC_PRIV_H__
#define __PHAL_G4_RCC_PRIV_H__

#include <stdint.h>

#include "stm32g474xx.h"
#include "common/phal_G4/rcc/rcc.h"


// This board's external oscillator input
// HSI16 (High Speed Internal, 16 MHz) is fixed by hardware
// HSE (High Speed External) depends on PCB/what is connected wired to the OSC_IN pin
// and PER only uses 16 MHZ HSE fed bypass style
static constexpr uint32_t RCC_PRIV_HSI_CLOCK_RATE_HZ = 16'000'000U;
static constexpr uint32_t RCC_PRIV_HSE_CLOCK_RATE_HZ = 16'000'000U;

// 170 MHz PLL
// VCO input  = HSI16 or HSE / PLLM  (must be 2.66-16 MHz)
// VCO output = VCO input * PLLN     (must be 96-344 MHz)
// PLL output = VCO output / PLLR    (-> SYSCLK)
static constexpr uint32_t RCC_PRIV_PLL_M = 4U;
static constexpr uint32_t RCC_PRIV_PLL_N = 85U;
static constexpr uint32_t RCC_PRIV_PLL_R = 2U;

static constexpr uint32_t RCC_PRIV_PLL_VCO_INPUT_HZ  = RCC_PRIV_HSI_CLOCK_RATE_HZ / RCC_PRIV_PLL_M;
static constexpr uint32_t RCC_PRIV_PLL_VCO_OUTPUT_HZ = RCC_PRIV_PLL_VCO_INPUT_HZ * RCC_PRIV_PLL_N;
static constexpr uint32_t RCC_PRIV_PLL_OUTPUT_HZ     = RCC_PRIV_PLL_VCO_OUTPUT_HZ / RCC_PRIV_PLL_R;

// Compile time check that chosen M/N/R choice is legal and results in a perfect 170 MHZ
static_assert(RCC_PRIV_HSI_CLOCK_RATE_HZ == RCC_PRIV_HSE_CLOCK_RATE_HZ, "PLL config assumes HSI and HSE");
static_assert(RCC_PRIV_PLL_VCO_INPUT_HZ >= 2'660'000U && RCC_PRIV_PLL_VCO_INPUT_HZ <= 16'000'000U, "PLL VCO input out of valid range");
static_assert(RCC_PRIV_PLL_VCO_OUTPUT_HZ >= 96'000'000U && RCC_PRIV_PLL_VCO_OUTPUT_HZ <= 344'000'000U, "PLL VCO output out of valid range");
static_assert(RCC_PRIV_PLL_OUTPUT_HZ == 170'000'000U, "PLL output must be exactly 170 MHz");

/// Oscillator source for RCC->PLLCFGR. Either HSI or HSE.
typedef enum : uint32_t {
    RCC_PRIV_PLL_SRC_HSI = RCC_PLLCFGR_PLLSRC_HSI,
    RCC_PRIV_PLL_SRC_HSE = RCC_PLLCFGR_PLLSRC_HSE,
} RCC_PRIV_PLLSource_t;

/// Enable HSI16 and block until it's ready
void PHAL_RCC_priv_enableHSI(void);

/// Disable HSI16 and block until it's off
void PHAL_RCC_priv_disableHSI(void);

/// Enable HSE in bypass mode (this board feeds it an external 16 MHz
/// signal directly, not a crystal) and block until it's ready
void PHAL_RCC_priv_enableHSE(void);

/// Set the flash wait states for the upcoming HCLK rate
/// Safe to call before increasing the clock (more wait states than currently
/// needed is fine)
void PHAL_RCC_priv_setFlashLatency(uint32_t latency_ws);

/// Configure and lock the PLL for RCC_PRIV_PLL_OUTPUT_HZ from the given source.
/// Does not touch SYSCLK. PLL runs, unselected, until PHAL_RCC_priv_switchSysclkToPLL
void PHAL_RCC_priv_configurePLL(RCC_PRIV_PLLSource_t src);

/// Enable Range 1 boost mode, required before SYSCLK can exceed 150 MHz
/// Call after the PLL is locked and before switching SYSCLK to it
void PHAL_RCC_priv_enableBoostMode(void);

/// Switch SYSCLK to HSI16 and block until the switch is confirmed
void PHAL_RCC_priv_switchSysclkToHSI(void);

/// Switch SYSCLK to HSE and block until the switch is confirmed
void PHAL_RCC_priv_switchSysclkToHSE(void);

/// Switch SYSCLK to the PLL and block until the switch is confirmed
void PHAL_RCC_priv_switchSysclkToPLL(void);

/// Set the AHB, APB1, and APB2 prescalers to divide-by-1 (undivided)
void PHAL_RCC_priv_setBusPrescalersToDiv1(void);

#endif // __PHAL_G4_RCC_PRIV_H__