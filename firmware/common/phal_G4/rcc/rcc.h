/**
 * @file rcc.h
 * @brief G4 RCC public API implementation.
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef __PHAL_G4_RCC_H__
#define __PHAL_G4_RCC_H__

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"

/**
 * @brief Supported system clock configurations.
 *
 * Every mode configures AHB, APB1, and APB2 to run undivided at the
 * resulting system clock rate
 */
typedef enum {
    PHAL_RCC_HSI_16MHZ,  /*!< HSI16 direct, no PLL, 16 MHz system clock   */
    PHAL_RCC_HSI_170MHZ, /*!< HSI16 through the PLL, 170 MHz (boost mode) */
    PHAL_RCC_HSE_16MHZ,  /*!< HSE direct, no PLL, 16 MHz system clock     */
    PHAL_RCC_HSE_170MHZ, /*!< HSE through the PLL, 170 MHz (boost mode)   */
} PHAL_RCC_Mode_t;

/**
 * @brief Configure the system, AHB, APB1, and APB2 clocks for a supported clock tree
 *
 * - The *_16MHZ modes run directly from the input oscillator (HSI16, or
 *   the board's 16 MHz HSE input in bypass mode) with no PLL
 *   - 0 Flash wait states
 *   - AHB = APB1 = APB2 = 16 MHz
 * - The *_170MHZ modes feed the input oscillator through the PLL
 *   (PLLM=4, PLLN=85, PLLR=2) for a clock speed of exactly 170 MHz
 *   - Enable Range 1 boost mode
 *   - Set 4 Flash wait states
 *   - AHB = APB1 = APB2 = 170 MHz
 *
 * The unselected input oscillator will be disabled once this function ends:
 * - HSI is on by default out of reset
 * - If HSI is selected, we don't turn it off
 * - For an HSE mode, HSI is switched off once it's no longer needed
 *
 * @param mode one of the four supported clock configurations
 */
void PHAL_RCC_init(PHAL_RCC_Mode_t mode);

/// @return the current system clock (SYSCLK) rate in Hz
uint32_t PHAL_RCC_getSystemClockHz(void);

/// @return the current AHB (HCLK) rate in Hz
uint32_t PHAL_RCC_getAHBClockHz(void);

/// @return the current APB1 (PCLK1) rate in Hz
uint32_t PHAL_RCC_getAPB1ClockHz(void);

/// @return the current APB2 (PCLK2) rate in Hz
uint32_t PHAL_RCC_getAPB2ClockHz(void);

#endif // __PHAL_G4_RCC_H__