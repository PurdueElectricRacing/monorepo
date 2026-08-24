/**
 * @file gpio_priv.h
 * @brief G4 GPIO private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */
#ifndef PHAL_G4_GPIO_PRIV_H
#define PHAL_G4_GPIO_PRIV_H

#include "common/phal_G4/gpio/gpio.h"

// GPIO config registers (MODER, OSPEEDR, OTYPER, PUPDR, AFR) pack one fixed-width
// field per pin, back-to-back starting at pin 0
static constexpr uint32_t GPIO_PRIV_MODER_FIELD_BITS   = 2U;
static constexpr uint32_t GPIO_PRIV_OSPEEDR_FIELD_BITS = 2U;
static constexpr uint32_t GPIO_PRIV_OTYPER_FIELD_BITS  = 1U;
static constexpr uint32_t GPIO_PRIV_PUPDR_FIELD_BITS   = 2U;
static constexpr uint32_t GPIO_PRIV_AFR_FIELD_BITS     = 4U;

// GPIOA-GPIOG sit back-to-back with each taking this many bytes of register space
static constexpr uint32_t GPIO_PRIV_PORT_STRIDE = 0x400U;

// Indexed by (bank_base - GPIOA_BASE) / GPIO_PRIV_PORT_STRIDE
// Each entry is the actual RCC enable bit for that port
static constexpr uint32_t GPIO_PRIV_RCC_ENABLE_BITS[] = {
    RCC_AHB2ENR_GPIOAEN,
    RCC_AHB2ENR_GPIOBEN,
    RCC_AHB2ENR_GPIOCEN,
    RCC_AHB2ENR_GPIODEN,
    RCC_AHB2ENR_GPIOEEN,
    RCC_AHB2ENR_GPIOFEN,
    RCC_AHB2ENR_GPIOGEN,
};
static constexpr uint32_t GPIO_PRIV_NUM_PORTS = sizeof(GPIO_PRIV_RCC_ENABLE_BITS) / sizeof(GPIO_PRIV_RCC_ENABLE_BITS[0]);

/**
 * @brief Enable the AHB2 clock for bank's port (GPIOA-GPIOG).
 * @return true on success; false if bank is not a recognized GPIO port
 */
bool PHAL_GPIO_priv_enableClock(GPIO_TypeDef *bank);

/// Set MODER's field for pin to type. Caller must have validated pin <= 15.
void PHAL_GPIO_priv_setMode(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_PinType_t type);

/// Set OSPEEDR's field for pin. Caller must have validated pin <= 15.
void PHAL_GPIO_priv_setOutputSpeed(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputSpeed_t speed);

/// Set OTYPER's field for pin. Caller must have validated pin <= 15.
void PHAL_GPIO_priv_setOutputType(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_OutputPull_t otype);

/// Set PUPDR's field for pin. Caller must have validated pin <= 15.
void PHAL_GPIO_priv_setPull(GPIO_TypeDef *bank, uint8_t pin, PHAL_GPIO_InputPull_t pull);

/// Set AFR's field for pin. Caller must have validated pin <= 15.
void PHAL_GPIO_priv_setAltFunction(GPIO_TypeDef *bank, uint8_t pin, uint8_t af_num);


static_assert(GPIO_MODER_MODE1_Pos - GPIO_MODER_MODE0_Pos == GPIO_PRIV_MODER_FIELD_BITS,
    "MODER field width doesn't match GPIO_PRIV_MODER_FIELD_BITS");
static_assert(GPIO_OSPEEDR_OSPEED1_Pos - GPIO_OSPEEDR_OSPEED0_Pos == GPIO_PRIV_OSPEEDR_FIELD_BITS,
    "OSPEEDR field width doesn't match GPIO_PRIV_OSPEEDR_FIELD_BITS");
static_assert(GPIO_OTYPER_OT1_Pos - GPIO_OTYPER_OT0_Pos == GPIO_PRIV_OTYPER_FIELD_BITS,
    "OTYPER field width doesn't match GPIO_PRIV_OTYPER_FIELD_BITS");
static_assert(GPIO_PUPDR_PUPD1_Pos - GPIO_PUPDR_PUPD0_Pos == GPIO_PRIV_PUPDR_FIELD_BITS,
    "PUPDR field width doesn't match GPIO_PRIV_PUPDR_FIELD_BITS");
static_assert(GPIO_AFRL_AFSEL1_Pos - GPIO_AFRL_AFSEL0_Pos == GPIO_PRIV_AFR_FIELD_BITS,
    "AFR field width doesn't match GPIO_PRIV_AFR_FIELD_BITS");

static_assert(GPIOB_BASE - GPIOA_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");
static_assert(GPIOC_BASE - GPIOB_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");
static_assert(GPIOD_BASE - GPIOC_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");
static_assert(GPIOE_BASE - GPIOD_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");
static_assert(GPIOF_BASE - GPIOE_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");
static_assert(GPIOG_BASE - GPIOF_BASE == GPIO_PRIV_PORT_STRIDE, "GPIO port base address spacing changed");

#endif // PHAL_G4_GPIO_PRIV_H