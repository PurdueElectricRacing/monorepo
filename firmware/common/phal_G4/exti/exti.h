/**
 * @file exti.h
 * @brief G4 EXTI public API
 * @author Ronak Jain (jain717@purdue.edu)
 */
#ifndef PHAL_G4_EXTI_H
#define PHAL_G4_EXTI_H

#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"

/**
 * @brief GPIO edge or edges that generate an EXTI interrupt.
 */
typedef enum : uint8_t {
    PHAL_EXTI_TRIGGER_RISING  = 0U,
    PHAL_EXTI_TRIGGER_FALLING = 1U,
    PHAL_EXTI_TRIGGER_BOTH    = 2U,
} PHAL_EXTI_Trigger_t;

/**
 * @brief Configuration for one GPIO-backed EXTI line.
 */
typedef struct {
    GPIO_TypeDef *bank;          /*!< GPIO port containing the input pin */
    uint8_t pin;                 /*!< Pin number, 0-15 */
    PHAL_EXTI_Trigger_t trigger; /*!< Edge that generates the interrupt */
    uint32_t irq_priority;       /*!< NVIC priority for this line's IRQ vector */
} PHAL_EXTI_InitConfig_t;

/**
 * @brief Configure GPIO EXTI lines from a configuration table.
 *
 * Configure the corresponding GPIO pins as inputs separately with
 * PHAL_GPIO_init(). Only pins 0-15 can be used, and each pin number can be
 * routed from one GPIO port at a time. Configuring a line selects its port,
 * trigger edge, interrupt mask, and shared NVIC interrupt. Entries are all
 * validated before any line is changed; duplicate pin numbers are rejected.
 * The IRQ priority is set from each entry's irq_priority. Lines 0-4 each
 * have a separate IRQ; lines 5-9 share EXTI9_5_IRQn and lines 10-15 share
 * EXTI15_10_IRQn. Entries sharing a vector must specify the same priority,
 * including any already-enabled sibling lines. Priorities must fit the device's
 * implemented NVIC priority bits.
 *
 * @param config Array of line configurations; may be nullptr when config_len is zero
 * @param config_len Number of entries in config
 * @return true if every configuration is valid and applied; false otherwise
 */
bool PHAL_EXTI_init(const PHAL_EXTI_InitConfig_t config[], size_t config_len);

/**
 * @brief Disable a configured GPIO EXTI line.
 *
 * The line is disabled only when its current SYSCFG route matches bank/pin.
 * A shared NVIC interrupt is disabled when no GPIO EXTI lines in its group
 * remain enabled.
 *
 * @param bank GPIO port containing the input pin
 * @param pin Pin number, 0-15
 * @return true if the route matched and the line was disabled; false otherwise
 */
bool PHAL_EXTI_deinit(GPIO_TypeDef *bank, uint8_t pin);

/**
 * @brief Weak callback invoked for each pending GPIO EXTI line.
 *
 * Called from an EXTI interrupt handler. Keep overrides short and non-blocking.
 * For both-edge triggers, sample the GPIO input to determine its current level.
 * The default implementation does nothing.
 *
 * @param bank GPIO port that generated the interrupt
 * @param pin Pin number, 0-15
 */
extern void PHAL_EXTI_callback(GPIO_TypeDef *bank, uint8_t pin);

#endif // PHAL_G4_EXTI_H
