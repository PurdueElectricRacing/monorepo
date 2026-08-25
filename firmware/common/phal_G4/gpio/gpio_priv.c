/**
 * @file gpio_priv.c
 * @brief G4 GPIO private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/gpio/gpio_priv.h"


/// Get the bit pattern for a pin's MODER field from its type
static uint32_t gpio_mode_bits(PHAL_GPIO_PinType_t type) {
    // MODER = port MODE Register
    switch (type) {
        case GPIO_TYPE_INPUT:  return 0b00;
        case GPIO_TYPE_OUTPUT: return 0b01;
        case GPIO_TYPE_AF:     return 0b10;
        case GPIO_TYPE_ANALOG: return 0b11;
        default:
            __builtin_unreachable();
    }
}

/// Get the bit pattern for a pin's OSPEEDR field from its speed
static uint32_t gpio_speed_bits(PHAL_GPIO_OutputSpeed_t speed) {
    // OSPEEDR = port Output SPEED Register
    switch (speed) {
        case GPIO_OUTPUT_LOW_SPEED:   return 0b00;
        case GPIO_OUTPUT_MED_SPEED:   return 0b01;
        case GPIO_OUTPUT_HIGH_SPEED:  return 0b10;
        case GPIO_OUTPUT_ULTRA_SPEED: return 0b11;
        default:
            __builtin_unreachable();
    }
}

/// Get the bit pattern for a pin's OTYPER field from its output type
static uint32_t gpio_otype_bits(PHAL_GPIO_OutputPull_t otype) {
    // OTYPER = port Output TYPE Register
    switch (otype) {
        case GPIO_OUTPUT_PUSH_PULL:  return 0b0;
        case GPIO_OUTPUT_OPEN_DRAIN: return 0b1;
        default:
            __builtin_unreachable();
    }
}

/// Get the bit pattern for a pin's PUPDR field from its pull selection
static uint32_t gpio_pull_bits(PHAL_GPIO_InputPull_t pull) {
    // PUPDR = port Pull-Up/Pull-Down Register
    switch (pull) {
        case GPIO_INPUT_OPEN_DRAIN: return 0b00;
        case GPIO_INPUT_PULL_UP:    return 0b01;
        case GPIO_INPUT_PULL_DOWN:  return 0b10;
        default:
            __builtin_unreachable();
    }
}

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
    uint32_t mode_bits = gpio_mode_bits(type);
    bank->MODER = (bank->MODER & ~(GPIO_MODER_MODE0_Msk << shift)) | (mode_bits << shift);
}

void PHAL_GPIO_priv_setOutputSpeed(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputSpeed_t speed) {
    // OSPEEDR = port Output SPEED Register
    // - slew rate / max toggle frequency per pin
    uint32_t shift = GPIO_PRIV_OSPEEDR_FIELD_BITS * pin;
    uint32_t speed_bits = gpio_speed_bits(speed);
    bank->OSPEEDR = (bank->OSPEEDR & ~(GPIO_OSPEEDR_OSPEED0_Msk << shift)) | (speed_bits << shift);
}

void PHAL_GPIO_priv_setOutputType(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputPull_t otype) {
    // OTYPER = port Output TYPE Register
    // - push-pull vs open-drain per pin
    uint32_t shift = GPIO_PRIV_OTYPER_FIELD_BITS * pin;
    uint32_t otype_bits = gpio_otype_bits(otype);
    bank->OTYPER = (bank->OTYPER & ~(GPIO_OTYPER_OT0_Msk << shift)) | (otype_bits << shift);
}

void PHAL_GPIO_priv_setPull(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_InputPull_t pull) {
    // PUPDR = port Pull-Up/Pull-Down Register
    uint32_t shift = GPIO_PRIV_PUPDR_FIELD_BITS * pin;
    uint32_t pull_bits = gpio_pull_bits(pull);
    bank->PUPDR = (bank->PUPDR & ~(GPIO_PUPDR_PUPD0_Msk << shift)) | (pull_bits << shift);
}

void PHAL_GPIO_priv_setAltFunction(GPIO_TypeDef *bank, uint8_t pin, uint8_t af_num) {
    // AFR = Alternate Function Register
    // - AFR[0] (AFRL) -> pins 0-7
    // - AFR[1] (AFRH) -> pins 8-15
    uint8_t reg    = (pin > 7U) ? 1U : 0U;
    uint32_t shift = GPIO_PRIV_AFR_FIELD_BITS * (pin % 8U);
    bank->AFR[reg] = (bank->AFR[reg] & ~(GPIO_AFRL_AFSEL0_Msk << shift)) | ((uint32_t)af_num << shift);
}


bool PHAL_GPIO_priv_read(const GPIO_TypeDef *bank, uint8_t pin) {
    // IDR = port Input Data Register 
    return (bank->IDR >> pin) & 0b1;
}
 
void PHAL_GPIO_priv_write(GPIO_TypeDef *bank, uint8_t pin, bool value) {
    // BSRR = port Bit Set/Reset Register
    // - BSRR's low 16 bits SET the corresponding pin, bits [31:16] RESET it
    // - value=true  -> !value=0 -> shift = pin      -> sets bit `pin` (SET)
    // - value=false -> !value=1 -> shift = 16 + pin -> sets bit `pin+16` (RESET)
    // - BSRR is write to trigger action so just set the register to the shifted value
    uint32_t bit_group = (!value << 4);
    uint32_t shift = bit_group | pin;
    bank->BSRR = 1U << shift;
}
 
void PHAL_GPIO_priv_toggle(GPIO_TypeDef *bank, uint8_t pin) {
    PHAL_GPIO_priv_write(bank, pin, !PHAL_GPIO_priv_read(bank, pin));
}