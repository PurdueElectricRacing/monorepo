/**
 * @file gpio.h
 * @brief G4 GPIO public API
 * @author Millan Kumar (kumar798@purdue.edu)
 */
#ifndef PHAL_G4_GPIO_H
#define PHAL_G4_GPIO_H

#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"

/**
 * @brief GPIO pin mode.
 *
 * Values match MODER's own 2-bit field encoding
 */
typedef enum {
    GPIO_TYPE_INPUT  = 0b00, /*!< Pin input mode */
    GPIO_TYPE_OUTPUT = 0b01, /*!< Pin output mode */
    GPIO_TYPE_AF     = 0b10, /*!< Pin alternate function mode */
    GPIO_TYPE_ANALOG = 0b11, /*!< Pin analog mode */
} PHAL_GPIO_PinType_t;

/**
 * @brief Output pin slew rate / maximum toggle frequency
 * 
 * Values match OSPEEDR's 2-bit field encoding
 */
typedef enum {
    GPIO_OUTPUT_LOW_SPEED   = 0b00, /*!< Slew rate control, max 8Mhz */
    GPIO_OUTPUT_MED_SPEED   = 0b01, /*!< Slew rate control, max 50Mhz */
    GPIO_OUTPUT_HIGH_SPEED  = 0b10, /*!< Slew rate control, max 100Mhz */
    GPIO_OUTPUT_ULTRA_SPEED = 0b11, /*!< Slew rate control, max 180Mhz */
} PHAL_GPIO_OutputSpeed_t;

// /** @brief Output pin drive mode. */
/**
 * @brief Output pin drive type
 * 
 * Values match OTYPER's 1-bit field encoding
 */
typedef enum {
    GPIO_OUTPUT_PUSH_PULL  = 0b0, /*!< Drive the output pin high and low */
    GPIO_OUTPUT_OPEN_DRAIN = 0b1, /*!< Drive the output pin low, high-z otherwise */
} PHAL_GPIO_OutputPull_t;

/**
 * @brief Input pin pull-up/pull-down resistor selection
 * 
 * Values match PUPDR's 2-bit field encoding
 */
typedef enum {
    GPIO_INPUT_OPEN_DRAIN = 0b00, /*!< No internal pull up/down */
    GPIO_INPUT_PULL_UP    = 0b01, /*!< Weak internal pull-up enabled */
    GPIO_INPUT_PULL_DOWN  = 0b10, /*!< Weak internal pull-down enabled */
} PHAL_GPIO_InputPull_t;

/**
 * @brief Configuration entry for GPIO initialization.
 */
typedef struct {
    GPIO_TypeDef *bank;       /*!< GPIO Bank for configuration */
    uint8_t pin;              /*!< Pin Number for configuration, 0-15 */
    PHAL_GPIO_PinType_t type; /*!< Mode of pin */

    struct {
        // INPUT ONLY FIELDS
        PHAL_GPIO_InputPull_t pull; /*!< Push/Pull selection */

        // OUTPUT ONLY FIELDS
        PHAL_GPIO_OutputSpeed_t ospeed; /*!< Output speed (slew rate) */
        PHAL_GPIO_OutputPull_t otype;   /*!< Output push/pull */
        // AF ONLY FIELDS
        uint8_t af_num; /*!< Alternate function number */
    } config; /*!< Type specific configuration for pins */
} PHAL_GPIO_InitConfig_t;

/**
 * @brief Initialize a set of GPIO pins from a configuration table
 *
 * Enables each pin's port clock, then configures mode, speed, drive type,
 * pull, and alternate function per entry as applicable to that entry's
 * PHAL_GPIO_PinType_t
 * 
 * Register fields that don't apply to a given type (ex: pull for an output pin)
 * are left untouched.
 *
 * When encountering an unknown bank, pin outside 0-15, or unrecognized type,
 * stops and returns false. Already configured pins remain configured.
 *
 * @param config Array of pin configurations
 * @param config_len Number of entries in config
 * @return true if every entry was valid (bank, pin #, type) and configured; false otherwise
 */
bool PHAL_GPIO_init(PHAL_GPIO_InitConfig_t config[], size_t config_len);

/**
 * @brief Read the current input state of a pin.
 * @param bank GPIO port
 * @param pin Pin number, 0-15
 * @return true if the pin currently reads high
 */
static inline bool PHAL_readGPIO(const GPIO_TypeDef *bank, uint8_t pin) {
    return (bank->IDR >> pin) & 0b1;
}

/**
 * @brief Drive an output pin high or low.
 *
 * Safe to call from an ISR concurrently with other threads/etc touching other
 * pins on the same port
 *
 * @param bank GPIO port
 * @param pin Pin number, 0-15
 * @param value true to drive high, false to drive low
 */
static inline void PHAL_writeGPIO(GPIO_TypeDef *bank, uint8_t pin, bool value) {
    // BSRR's low 16 bits SET the corresponding pin
    // bits [31:16] RESET it
    // value=true  -> !value=0 -> shift = pin      - > sets bit `pin` (SET)
    // value=false -> !value=1 -> shift = 16 + pin  -> sets bit `pin+16` (RESET)
    bank->BSRR |= 1 << ((!value << 4) | pin);
}

// /** @brief Flip an output pin's current state. */
/**
 * @brief Flip an output pin's current state.
 * 
 * Works by reading the pin's current state and writing the opposite value.
 *
 * @param bank GPIO port
 * @param pin Pin number, 0-15
 */
static inline void PHAL_toggleGPIO(GPIO_TypeDef *bank, uint8_t pin) {
    PHAL_writeGPIO(bank, pin, !PHAL_readGPIO(bank, pin));
}

/**
 * @brief Create a GPIO Init struct entry to initialize a pin for input.
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param input_pull_sel Pull-up/pull-down/high-z selection
 */
#define GPIO_INIT_INPUT(gpio_bank, pin_num, input_pull_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_INPUT, .config = { \
            .pull = input_pull_sel \
        } \
    }

/**
 * @brief Create a GPIO Init struct entry to initialize a pin for output.
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param ospeed_sel Output speed selection
 */
#define GPIO_INIT_OUTPUT(gpio_bank, pin_num, ospeed_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_OUTPUT, .config = { \
            .ospeed = ospeed_sel, \
            .otype  = GPIO_OUTPUT_PUSH_PULL \
        } \
    }

#define GPIO_INIT_OUTPUT_OPEN_DRAIN(gpio_bank, pin_num, ospeed_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_OUTPUT, .config = { \
            .ospeed = ospeed_sel, \
            .otype  = GPIO_OUTPUT_OPEN_DRAIN \
        } \
    }

/**
 * @brief Create a GPIO Init struct entry to initialize a pin for analog.
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 */
#define GPIO_INIT_ANALOG(gpio_bank, pin_num) \
    {.bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_ANALOG}

/**
 * @brief Create a GPIO Init struct entry to initialize a pin for alternate function.
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param alt_func_num Alternate function selection
 * @param ospeed_sel Output speed selection
 * @param otype_sel Output drive type selection
 * @param input_pull_sel Pull-up/pull-down/high-z selection
 */
#define GPIO_INIT_AF(gpio_bank, pin_num, alt_func_num, ospeed_sel, otype_sel, input_pull_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_AF, .config = { \
            .af_num = alt_func_num, \
            .ospeed = ospeed_sel, \
            .otype  = otype_sel, \
            .pull   = input_pull_sel \
        } \
    }

/*
    Useful defines for GPIO Init struct with commonly used peripheral/pin mappings.
    If you find yourself adding the same pin mappings to multiple devices, add a macro below
    to cut down on duplication.
*/

#define GPIO_INIT_USART3TX_PC10 \
    GPIO_INIT_AF(GPIOC, \
                 10, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART3RX_PC11 \
    GPIO_INIT_AF(GPIOC, \
                 11, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART3TX_PB10 \
    GPIO_INIT_AF(GPIOB, \
                 10, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART3RX_PB11 \
    GPIO_INIT_AF(GPIOB, \
                 11, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART2TX_PA2 \
    GPIO_INIT_AF(GPIOA, 2, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART2RX_PA3 \
    GPIO_INIT_AF(GPIOA, \
                 3, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART1TX_PA9 \
    GPIO_INIT_AF(GPIOA, 9, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART1RX_PA10 \
    GPIO_INIT_AF(GPIOA, \
                 10, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART2TX_PD5 \
    GPIO_INIT_AF(GPIOD, 5, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART2RX_PD6 \
    GPIO_INIT_AF(GPIOD, \
                 6, \
                 7, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_UART4TX_PC10 \
    GPIO_INIT_AF(GPIOC, \
                 10, \
                 8, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_UART4RX_PC11 \
    GPIO_INIT_AF(GPIOC, \
                 11, \
                 8, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_LPUART1TX_PC0 \
    GPIO_INIT_AF(GPIOC, 0, 8, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_LPUART1RX_PC1 \
    GPIO_INIT_AF(GPIOC, \
                 1, \
                 8, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_OPEN_DRAIN, \
                 GPIO_INPUT_OPEN_DRAIN)

/* * SPI1 Pins (Standard for both RET and CET)
 * PA5=SCK, PA6=MISO, PA7=MOSI, PA4 or PA15=NSS (AF5)
 */
#define GPIO_INIT_SPI1SCK_PA5 \
    GPIO_INIT_AF(GPIOA, 5, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI1MISO_PA6 \
    GPIO_INIT_AF(GPIOA, 6, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)

#define GPIO_INIT_SPI1MOSI_PA7 \
    GPIO_INIT_AF(GPIOA, 7, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI1NSS_PA4 \
    GPIO_INIT_AF(GPIOA, 4, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI1NSS_PA15 \
    GPIO_INIT_AF(GPIOA, \
                 15, \
                 5, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)

/* * SPI2 Pins - RET Package (64-pin)
 * PB13=SCK, PB14=MISO, PB15=MOSI, PB12=NSS (AF5)
 */
#define GPIO_INIT_SPI2SCK_RET_PB13 \
    GPIO_INIT_AF(GPIOB, \
                 13, \
                 5, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI2MISO_RET_PB14 \
    GPIO_INIT_AF(GPIOB, 14, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)

#define GPIO_INIT_SPI2MOSI_RET_PB15 \
    GPIO_INIT_AF(GPIOB, \
                 15, \
                 5, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI2NSS_RET_PB12 \
    GPIO_INIT_AF(GPIOB, \
                 12, \
                 5, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)

/* * SPI2 Pins - CET Package (48-pin)
 * PA9=SCK, PA10=MISO, PA11=MOSI, PA8=NSS (AF5)
 */
#define GPIO_INIT_SPI2SCK_CET_PA9 \
    GPIO_INIT_AF(GPIOA, 9, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI2MISO_CET_PA10 \
    GPIO_INIT_AF(GPIOA, 10, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)

#define GPIO_INIT_SPI2MOSI_CET_PA11 \
    GPIO_INIT_AF(GPIOA, \
                 11, \
                 5, \
                 GPIO_OUTPUT_ULTRA_SPEED, \
                 GPIO_OUTPUT_PUSH_PULL, \
                 GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_SPI2NSS_CET_PA8 \
    GPIO_INIT_AF(GPIOA, 8, 5, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#endif // PHAL_G4_GPIO_H