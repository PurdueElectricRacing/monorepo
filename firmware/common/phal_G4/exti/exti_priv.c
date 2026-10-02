/**
 * @file exti_priv.c
 * @brief G4 EXTI private/register-level implementation
 * @author Ronak Jain (jain717@purdue.edu)
 */
#include "common/phal_G4/exti/exti_priv.h"

bool PHAL_EXTI_priv_getPortIndex(const GPIO_TypeDef *bank, uint8_t *port_index) {
    if (bank == GPIOA) {
        *port_index = 0U;
    } else if (bank == GPIOB) {
        *port_index = 1U;
    } else if (bank == GPIOC) {
        *port_index = 2U;
    } else if (bank == GPIOD) {
        *port_index = 3U;
    } else if (bank == GPIOE) {
        *port_index = 4U;
    } else if (bank == GPIOF) {
        *port_index = 5U;
    } else if (bank == GPIOG) {
        *port_index = 6U;
    } else {
        return false;
    }

    return true;
}

void PHAL_EXTI_priv_enableClock(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

void PHAL_EXTI_priv_configureLine(uint8_t pin, uint8_t port_index, PHAL_EXTI_Trigger_t trigger) {
    uint32_t line_mask    = 1U << pin;
    uint32_t exticr_idx   = pin / 4U;
    uint32_t exticr_shift = (pin % 4U) * 4U;
    uint32_t exticr_mask  = SYSCFG_EXTICR1_EXTI0_Msk << exticr_shift;

    EXTI->IMR1 &= ~line_mask;

    SYSCFG->EXTICR[exticr_idx] =
        (SYSCFG->EXTICR[exticr_idx] & ~exticr_mask) | ((uint32_t)port_index << exticr_shift);

    if (trigger == PHAL_EXTI_TRIGGER_RISING || trigger == PHAL_EXTI_TRIGGER_BOTH) {
        EXTI->RTSR1 |= line_mask;
    } else {
        EXTI->RTSR1 &= ~line_mask;
    }

    if (trigger == PHAL_EXTI_TRIGGER_FALLING || trigger == PHAL_EXTI_TRIGGER_BOTH) {
        EXTI->FTSR1 |= line_mask;
    } else {
        EXTI->FTSR1 &= ~line_mask;
    }

    PHAL_EXTI_priv_clearPending(pin);
    EXTI->IMR1 |= line_mask;
}

void PHAL_EXTI_priv_disableLine(uint8_t pin) {
    uint32_t line_mask = 1U << pin;
    EXTI->IMR1 &= ~line_mask;
    EXTI->RTSR1 &= ~line_mask;
    EXTI->FTSR1 &= ~line_mask;
    PHAL_EXTI_priv_clearPending(pin);
}

uint32_t PHAL_EXTI_priv_getPendingLines(uint32_t line_mask) {
    return EXTI->PR1 & EXTI->IMR1 & line_mask;
}

void PHAL_EXTI_priv_clearPending(uint8_t pin) {
    EXTI->PR1 = 1U << pin;
}

GPIO_TypeDef *PHAL_EXTI_priv_getBank(uint8_t pin) {
    uint32_t exticr_idx   = pin / 4U;
    uint32_t exticr_shift = (pin % 4U) * 4U;
    uint32_t port_index   = (SYSCFG->EXTICR[exticr_idx] >> exticr_shift) & 0x7U;

    switch (port_index) {
        case 0U:
            return GPIOA;
        case 1U:
            return GPIOB;
        case 2U:
            return GPIOC;
        case 3U:
            return GPIOD;
        case 4U:
            return GPIOE;
        case 5U:
            return GPIOF;
        case 6U:
            return GPIOG;
        default:
            return nullptr;
    }
}

IRQn_Type PHAL_EXTI_priv_getIRQn(uint8_t pin) {
    if (pin <= 4U) {
        return (IRQn_Type)(EXTI0_IRQn + pin);
    }
    if (pin <= 9U) {
        return EXTI9_5_IRQn;
    }
    return EXTI15_10_IRQn;
}
