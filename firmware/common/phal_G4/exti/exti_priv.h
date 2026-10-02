/**
 * @file exti_priv.h
 * @brief G4 EXTI private/register-level implementation
 * @author Ronak Jain (jain717@purdue.edu)
 */
#ifndef PHAL_G4_EXTI_PRIV_H
#define PHAL_G4_EXTI_PRIV_H

#include "common/phal_G4/exti/exti.h"

static constexpr uint32_t PHAL_EXTI_PRIV_GPIO_LINE_COUNT = 16U;

/// Return the SYSCFG port selector for a supported GPIO bank (GPIOA-GPIOG).
bool PHAL_EXTI_priv_getPortIndex(const GPIO_TypeDef *bank, uint8_t *port_index);

/// Enable the SYSCFG peripheral clock used for GPIO EXTI routing.
void PHAL_EXTI_priv_enableClock(void);

/// Mask a line, route it, configure its trigger, clear stale pending state, and unmask it.
void PHAL_EXTI_priv_configureLine(uint8_t pin, uint8_t port_index, PHAL_EXTI_Trigger_t trigger);

/// Disable a line's interrupt and edge triggers, then clear its pending flag.
void PHAL_EXTI_priv_disableLine(uint8_t pin);

/// Read pending, unmasked GPIO EXTI lines selected by line_mask.
uint32_t PHAL_EXTI_priv_getPendingLines(uint32_t line_mask);

/// Clear one line's pending flag using EXTI's write-one-to-clear semantics.
void PHAL_EXTI_priv_clearPending(uint8_t pin);

/// Return the GPIO bank currently selected by SYSCFG for pin, or nullptr if invalid.
GPIO_TypeDef *PHAL_EXTI_priv_getBank(uint8_t pin);

/// Return the NVIC interrupt shared by pin (0-15).
IRQn_Type PHAL_EXTI_priv_getIRQn(uint8_t pin);

#endif // PHAL_G4_EXTI_PRIV_H
