/**
 * @file exti.c
 * @brief G4 EXTI public API implementation
 * @author Ronak Jain (jain717@purdue.edu)
 */
#include "common/phal_G4/exti/exti.h"

#include "common/phal_G4/exti/exti_priv.h"

static bool exti_validate_config(const PHAL_EXTI_InitConfig_t config[], size_t config_len) {
    uint32_t configured_lines = 0U;
    for (size_t i = 0U; i < config_len; i++) {
        uint8_t port_index = 0U;
        if (config[i].pin >= PHAL_EXTI_PRIV_GPIO_LINE_COUNT
            || config[i].irq_priority >= (1U << __NVIC_PRIO_BITS)
            || !PHAL_EXTI_priv_getPortIndex(config[i].bank, &port_index)) {
            return false;
        }

        uint32_t line_mask = 1U << config[i].pin;
        if ((configured_lines & line_mask) != 0U) {
            return false;
        }
        configured_lines |= line_mask;

        // An NVIC priority belongs to the vector, not an individual EXTI line.
        IRQn_Type irq = PHAL_EXTI_priv_getIRQn(config[i].pin);
        uint32_t enabled_siblings = PHAL_EXTI_priv_getEnabledIRQGroupLines(config[i].pin) & ~line_mask;
        if (enabled_siblings != 0U && NVIC_GetPriority(irq) != config[i].irq_priority) {
            return false;
        }
        for (size_t j = 0U; j < i; j++) {
            if (PHAL_EXTI_priv_getIRQn(config[j].pin) == irq
                && config[j].irq_priority != config[i].irq_priority) {
                return false;
            }
        }
    }

    return true;
}

bool PHAL_EXTI_init(const PHAL_EXTI_InitConfig_t config[], size_t config_len) {
    if (config_len == 0U) {
        return true;
    }
    if (config == nullptr || !exti_validate_config(config, config_len)) {
        return false;
    }

    // Validate the entire configuration before changing any hardware.
    PHAL_EXTI_priv_enableClock();
    for (size_t i = 0U; i < config_len; i++) {
        uint8_t port_index = 0U;
        PHAL_EXTI_priv_getPortIndex(config[i].bank, &port_index);
        PHAL_EXTI_priv_configureLine(config[i].pin, port_index, config[i].trigger);
    }

    for (size_t i = 0U; i < config_len; i++) {
        IRQn_Type irq = PHAL_EXTI_priv_getIRQn(config[i].pin);
        NVIC_SetPriority(irq, config[i].irq_priority);
        NVIC_EnableIRQ(irq);
    }

    return true;
}

bool PHAL_EXTI_deinit(GPIO_TypeDef *bank, uint8_t pin) {
    if (pin >= PHAL_EXTI_PRIV_GPIO_LINE_COUNT || bank == nullptr) {
        return false;
    }

    PHAL_EXTI_priv_enableClock();
    if (PHAL_EXTI_priv_getBank(pin) != bank) {
        return false;
    }

    PHAL_EXTI_priv_disableLine(pin);

    IRQn_Type irq = PHAL_EXTI_priv_getIRQn(pin);
    if (PHAL_EXTI_priv_getEnabledIRQGroupLines(pin) == 0U) {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }

    return true;
}

static void exti_dispatch(uint32_t line_mask) {
    uint32_t pending_lines = PHAL_EXTI_priv_getPendingLines(line_mask);

    for (uint8_t pin = 0U; pin < PHAL_EXTI_PRIV_GPIO_LINE_COUNT; pin++) {
        uint32_t current_line_mask = 1U << pin;
        if ((pending_lines & current_line_mask) == 0U
            || (PHAL_EXTI_priv_getPendingLines(current_line_mask) == 0U)) {
            continue;
        }

        PHAL_EXTI_priv_clearPending(pin);
        GPIO_TypeDef *bank = PHAL_EXTI_priv_getBank(pin);
        if (bank != nullptr) {
            PHAL_EXTI_callback(bank, pin);
        }
    }
}

[[gnu::weak]]
void PHAL_EXTI_callback(GPIO_TypeDef *bank, uint8_t pin) {
    (void)bank;
    (void)pin;
}

void EXTI0_IRQHandler(void) {
    exti_dispatch(0x0001U);
}

void EXTI1_IRQHandler(void) {
    exti_dispatch(0x0002U);
}

void EXTI2_IRQHandler(void) {
    exti_dispatch(0x0004U);
}

void EXTI3_IRQHandler(void) {
    exti_dispatch(0x0008U);
}

void EXTI4_IRQHandler(void) {
    exti_dispatch(0x0010U);
}

void EXTI9_5_IRQHandler(void) {
    exti_dispatch(0x03E0U);
}

void EXTI15_10_IRQHandler(void) {
    exti_dispatch(0xFC00U);
}
