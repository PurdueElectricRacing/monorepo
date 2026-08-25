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
 * Is the key for the config tagged union in PHAL_GPIO_PinType_t
 */
typedef enum {
    GPIO_TYPE_INPUT,
    GPIO_TYPE_OUTPUT,
    GPIO_TYPE_AF,
    GPIO_TYPE_ANALOG,
} PHAL_GPIO_PinType_t;

/**
 * @brief Output pin slew rate / maximum toggle frequency.
 */
typedef enum {
    GPIO_OUTPUT_LOW_SPEED,   /*!< Slew rate control, max 8Mhz */
    GPIO_OUTPUT_MED_SPEED,   /*!< Slew rate control, max 50Mhz */
    GPIO_OUTPUT_HIGH_SPEED,  /*!< Slew rate control, max 100Mhz */
    GPIO_OUTPUT_ULTRA_SPEED, /*!< Slew rate control, max 180Mhz */
} PHAL_GPIO_OutputSpeed_t;
/**
 * @brief Output pin drive type.
 */
typedef enum {
    GPIO_OUTPUT_PUSH_PULL,  /*!< Drive the output pin high and low */
    GPIO_OUTPUT_OPEN_DRAIN, /*!< Drive the output pin low, high-z otherwise */
} PHAL_GPIO_OutputPull_t;

/**
 * @brief Input pin pull-up/pull-down resistor selection.
 */
typedef enum {
    GPIO_INPUT_OPEN_DRAIN, /*!< No internal pull up/down */
    GPIO_INPUT_PULL_UP,    /*!< Weak internal pull-up enabled */
    GPIO_INPUT_PULL_DOWN,  /*!< Weak internal pull-down enabled */
} PHAL_GPIO_InputPull_t;


/**
 * @brief Configuration entry for GPIO initialization.
 *
 * `config` is a tagged union keyed by PHAL_GPIO_PinType_t type
 */
typedef struct {
    GPIO_TypeDef *bank;       /*!< GPIO Bank for configuration */
    uint8_t pin;              /*!< Pin Number for configuration, 0-15 */
    PHAL_GPIO_PinType_t type; /*!< Mode of pin */
 
    union {
        struct {
            PHAL_GPIO_InputPull_t pull; /*!< Pull-up/pull-down selection */
        } input; /*!< Valid when type == GPIO_TYPE_INPUT */
 
        struct {
            PHAL_GPIO_OutputSpeed_t ospeed; /*!< Output speed (slew rate) */
            PHAL_GPIO_OutputPull_t otype;   /*!< Output push/pull */
        } output; /*!< Valid when type == GPIO_TYPE_OUTPUT */
 
        struct {
            uint8_t af_num;                 /*!< Alternate function number */
            PHAL_GPIO_OutputSpeed_t ospeed; /*!< Output speed (slew rate) */
            PHAL_GPIO_OutputPull_t otype;   /*!< Output push/pull */
            PHAL_GPIO_InputPull_t pull;     /*!< Pull-up/pull-down selection */
        } af; /*!< Valid when type == GPIO_TYPE_AF */
 
        // GPIO_TYPE_ANALOG needs no additional configuration
    } config; /*!< Type-specific configuration for the pin */
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
bool PHAL_GPIO_read(const GPIO_TypeDef *bank, uint8_t pin);

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
void PHAL_GPIO_write(GPIO_TypeDef *bank, uint8_t pin, bool value);

/**
 * @brief Flip an output pin's current state.
 * @param bank GPIO port
 * @param pin Pin number, 0-15
 */
void PHAL_GPIO_toggle(GPIO_TypeDef *bank, uint8_t pin);

/**
 * @brief Construct PHAL_GPIO_InitConfig_t for an input pin
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param input_pull_sel Pull-up/pull-down/high-z selection
 */
#define PHAL_GPIO_INIT_INPUT(gpio_bank, pin_num, input_pull_sel)                    \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .type = GPIO_TYPE_INPUT,                                               \
        .config = {                                                            \
            .input = {                                                         \
                .pull = input_pull_sel                                         \
            }                                                                  \
        }                                                                      \
    }

/**
 * @brief Construct PHAL_GPIO_InitConfig_t for an output (push-pull) pin
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param ospeed_sel Output speed selection
 */
#define PHAL_GPIO_INIT_OUTPUT(gpio_bank, pin_num, ospeed_sel)                  \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .type = GPIO_TYPE_OUTPUT,                                              \
        .config = {                                                            \
            .output = {                                                        \
                .ospeed = ospeed_sel,                                          \
                .otype  = GPIO_OUTPUT_PUSH_PULL                                \
            }                                                                  \
        }                                                                      \
    }

/**
 * @brief Construct PHAL_GPIO_InitConfig_t for an output (open-drain) pin
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param ospeed_sel Output speed selection
 */
#define PHAL_GPIO_INIT_OUTPUT_OPEN_DRAIN(gpio_bank, pin_num, ospeed_sel)       \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .type = GPIO_TYPE_OUTPUT,                                              \
        .config = {                                                            \
            .output = {                                                        \
                .ospeed = ospeed_sel,                                          \
                .otype  = GPIO_OUTPUT_OPEN_DRAIN                               \
            }                                                                  \
        }                                                                      \
    }

/**
 * @brief Construct PHAL_GPIO_InitConfig_t for an analog pin
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 */
#define PHAL_GPIO_INIT_ANALOG(gpio_bank, pin_num)                              \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .type = GPIO_TYPE_ANALOG                                               \
    }

/**
 * @brief Construct PHAL_GPIO_InitConfig_t for an alternate function pin
 * @param gpio_bank GPIO_TypeDef* for the pin's port
 * @param pin_num Pin number, 0-15
 * @param alt_func_num Alternate function selection
 * @param ospeed_sel Output speed selection
 * @param otype_sel Output drive type selection
 * @param input_pull_sel Pull-up/pull-down/high-z selection
 */
#define PHAL_GPIO_INIT_AF(gpio_bank, pin_num, alt_func_num, ospeed_sel, otype_sel, input_pull_sel) \
    {                                                                                              \
        .bank = gpio_bank,                                                                         \
        .pin = pin_num,                                                                            \
        .type = GPIO_TYPE_AF,                                                                      \
        .config = {                                                                                \
            .af = {                                                                                \
                .af_num = alt_func_num,                                                            \
                .ospeed = ospeed_sel,                                                              \
                .otype  = otype_sel,                                                               \
                .pull   = input_pull_sel                                                           \
            }                                                                                      \
        }                                                                                          \
    }

#endif // PHAL_G4_GPIO_H