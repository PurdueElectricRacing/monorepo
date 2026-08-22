/**
 * @file rcc.c
 * @brief G4 RCC public API implementation.
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/rcc/rcc.h"

#include "common/phal_G4/rcc/rcc_priv.h"


// Init to safe defaults, all HSI 16 MHz
static uint32_t g_system_clock_hz = RCC_PRIV_HSI_CLOCK_RATE_HZ;
static uint32_t g_ahb_clock_hz    = RCC_PRIV_HSI_CLOCK_RATE_HZ;
static uint32_t g_apb1_clock_hz   = RCC_PRIV_HSI_CLOCK_RATE_HZ;
static uint32_t g_apb2_clock_hz   = RCC_PRIV_HSI_CLOCK_RATE_HZ;


static void rcc_initFromOscillator(bool use_hse, bool use_pll) {
    if (use_hse) {
        PHAL_RCC_priv_enableHSE();
    } else {
        PHAL_RCC_priv_enableHSI();
    }

    if (use_pll) {
        RCC_PRIV_PLLSource_t pll_src = use_hse ? RCC_PRIV_PLL_SRC_HSE : RCC_PRIV_PLL_SRC_HSI;
        PHAL_RCC_priv_configurePLL(pll_src);
        PHAL_RCC_priv_setFlashLatency(FLASH_ACR_LATENCY_4WS);
        PHAL_RCC_priv_enableBoostMode();
        PHAL_RCC_priv_switchSysclkToPLL();
        g_system_clock_hz = RCC_PRIV_PLL_OUTPUT_HZ;
    } else {
        PHAL_RCC_priv_setFlashLatency(FLASH_ACR_LATENCY_0WS);
        if (use_hse) {
            PHAL_RCC_priv_switchSysclkToHSE();
        } else {
            PHAL_RCC_priv_switchSysclkToHSI();
        }
        // HSE and HSI could be different Hz
        // cppcheck-suppress duplicateExpressionTernary
        g_system_clock_hz = use_hse ? RCC_PRIV_HSE_CLOCK_RATE_HZ : RCC_PRIV_HSI_CLOCK_RATE_HZ;
    }

    if (use_hse) {
        // No longer need HSI once running from HSE
        PHAL_RCC_priv_disableHSI(); 
    }

    PHAL_RCC_priv_setBusPrescalersToDiv1();
    g_ahb_clock_hz  = g_system_clock_hz;
    g_apb1_clock_hz = g_system_clock_hz;
    g_apb2_clock_hz = g_system_clock_hz;

    SystemCoreClockUpdate();
}

void PHAL_RCC_init(PHAL_RCC_Mode_t mode) {
    switch (mode) {
        case PHAL_RCC_HSI_16MHZ:  rcc_initFromOscillator(false, false); break;
        case PHAL_RCC_HSI_170MHZ: rcc_initFromOscillator(false, true);  break;
        case PHAL_RCC_HSE_16MHZ:  rcc_initFromOscillator(true, false);  break;
        case PHAL_RCC_HSE_170MHZ: rcc_initFromOscillator(true, true);   break;
        default:
            __builtin_trap();
    }
}

uint32_t PHAL_RCC_getSystemClockHz(void) {
    return g_system_clock_hz;
}

uint32_t PHAL_RCC_getAHBClockHz(void) {
    return g_ahb_clock_hz;
}

uint32_t PHAL_RCC_getAPB1ClockHz(void) {
    return g_apb1_clock_hz;
}

uint32_t PHAL_RCC_getAPB2ClockHz(void) {
    return g_apb2_clock_hz;
}
