/**
 * @file gpio_priv.c
 * @brief G4 GPIO private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/gpio/gpio_priv.h"

bool PHAL_GPIO_priv_enableClock(GPIO_TypeDef *bank) {
    uint32_t offset = (uint32_t)bank - GPIOA_BASE;

    // Not one of GPIOA-GPIOG's base addresses
    if (offset % GPIO_PRIV_PORT_STRIDE != 0U) {
        return false;
    }

    uint32_t index = offset / GPIO_PRIV_PORT_STRIDE;
    if (index >= GPIO_PRIV_NUM_PORTS) {
        return false;
    }

    // AHB2ENR = AHB2 peripheral clock Enable Register
    RCC->AHB2ENR |= GPIO_PRIV_RCC_ENABLE_BITS[index];
    return true;
}

void PHAL_GPIO_priv_setMode(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_PinType_t type) {
    // MODER = port MODE Register
    // - selects input/output/AF/analog per pin
    uint32_t shift = GPIO_PRIV_MODER_FIELD_BITS * pin;
    bank->MODER = (bank->MODER & ~(GPIO_MODER_MODE0_Msk << shift)) | ((uint32_t)type << shift);
}

void PHAL_GPIO_priv_setOutputSpeed(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputSpeed_t speed) {
    // OSPEEDR = port Output SPEED Register
    // - slew rate / max toggle frequency per pin
    uint32_t shift = GPIO_PRIV_OSPEEDR_FIELD_BITS * pin;
    bank->OSPEEDR = (bank->OSPEEDR & ~(GPIO_OSPEEDR_OSPEED0_Msk << shift)) | ((uint32_t)speed << shift);
}

void PHAL_GPIO_priv_setOutputType(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputPull_t otype) {
    // OTYPER = port Output TYPE Register
    // - push-pull vs open-drain per pin
    uint32_t shift = GPIO_PRIV_OTYPER_FIELD_BITS * pin;
    bank->OTYPER = (bank->OTYPER & ~(GPIO_OTYPER_OT0_Msk << shift)) | ((uint32_t)otype << shift);
}

void PHAL_GPIO_priv_setPull(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_InputPull_t pull) {
    // PUPDR = port Pull-Up/Pull-Down Register
    uint32_t shift = GPIO_PRIV_PUPDR_FIELD_BITS * pin;
    bank->PUPDR = (bank->PUPDR & ~(GPIO_PUPDR_PUPD0_Msk << shift)) | ((uint32_t)pull << shift);
}

void PHAL_GPIO_priv_setAltFunction(GPIO_TypeDef *bank, uint8_t pin, uint8_t af_num) {
    // AFR = Alternate Function Register
    // - AFR[0] (AFRL) -> pins 0-7
    // - AFR[1] (AFRH) -> pins 8-15
    uint8_t reg    = (pin > 7U) ? 1U : 0U;
    uint32_t shift = GPIO_PRIV_AFR_FIELD_BITS * (pin % 8U);
    bank->AFR[reg] = (bank->AFR[reg] & ~(GPIO_AFRL_AFSEL0_Msk << shift)) | ((uint32_t)af_num << shift);
}