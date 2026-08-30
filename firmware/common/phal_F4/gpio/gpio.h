/**
 * @file gpio.h
 * @author Adam Busch (busch8@purdue.edu)
 * @brief GPIO Driver for STM32F4 Devices
 * @version 0.1
 * @date 2021-09-20
 *
 *
 * @copyright Copyright (c) 2021
 *
 *
 */
#ifndef _PHAL_GPIO_H_
#define _PHAL_GPIO_H_

#include "common/phal_F4/phal_F4.h"

/**
 * @brief Configuration type for GPIO Pin
 */
typedef enum {
    GPIO_TYPE_INPUT  = 0b00, /* Pin input mode */
    GPIO_TYPE_OUTPUT = 0b01, /* Pin output mode */
    GPIO_TYPE_AF     = 0b10, /* Pin alternate function mode */
    GPIO_TYPE_ANALOG = 0b11, /* Pin alternate function mode */
} GPIOPinType_t;

/**
 * @brief Slew rate control for output pins
 */
typedef enum {
    GPIO_OUTPUT_LOW_SPEED   = 0b00, /* Slew rate control, max 8Mhz */
    GPIO_OUTPUT_MED_SPEED   = 0b01, /* Slew rate control, max 50Mhz */
    GPIO_OUTPUT_HIGH_SPEED  = 0b10, /* Slew rate control, max 100Mhz */
    GPIO_OUTPUT_ULTRA_SPEED = 0b11, /* Slew rate control, max 180Mhz */
} GPIOOutputSpeed_t;

/**
 * @brief Output drive mode selection
 */
typedef enum {
    GPIO_OUTPUT_PUSH_PULL  = 0b0, /* Drive the output pin high and low */
    GPIO_OUTPUT_OPEN_DRAIN = 0b1, /* Drive the output pin low, high-z otherwise */
} GPIOOutputPull_t;

/**
 * @brief Enable internal pullup/down resistors
 */
typedef enum {
    GPIO_INPUT_OPEN_DRAIN = 0b00, /* No internal pull up/down */
    GPIO_INPUT_PULL_UP    = 0b01, /* Weak internal pull-up enabled */
    GPIO_INPUT_PULL_DOWN  = 0b10, /* Weak internal pull-down enabled */
} GPIOInputPull_t;

/**
 * @brief Configuration entry for GPIO initilization
 */
typedef struct
{
    GPIO_TypeDef* bank; /* GPIO Bank for configuration */
    uint8_t pin; /* Pin Number for configruation */
    GPIOPinType_t type; /* Output type of pin */

    struct
    {
        // INPUT ONLY FIELDS
        GPIOInputPull_t pull; /* Push/Pull selection */

        // OUTPUT ONLY FIELDS
        GPIOOutputSpeed_t ospeed; /* Output speed (slew rate) */
        GPIOOutputPull_t otype; /* Output push/pull */

        // AF ONLY FIELDS
        uint8_t af_num; /* Anternate function type */
    } config; /* Type specific configuration for pins */
} GPIOInitConfig_t;

/**
 * @brief Create GPIO Init struct to intilize a GPIO pin for input
 *
 * @param gpio_bank GPIO_TypeDef* reference to the GPIO bank for the pin
 * @param pin_num Pin number from GPIO bank to configure
 * @param input_pull_sel Input pullup/pulldown/high-z selection
 */
#define GPIO_INIT_INPUT(gpio_bank, pin_num, input_pull_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_INPUT, .config = {.pull = input_pull_sel } \
    }

/**
 * @brief Create GPIO Init struct to intilize a GPIO pin for output
 *
 * @param gpio_bank GPIO_TypeDef* reference to the GPIO bank for the pin
 * @param pin_num Pin number from GPIO bank to configure
 * @param ospeed_sel Pin output speed selection
 */
#define GPIO_INIT_OUTPUT(gpio_bank, pin_num, ospeed_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_OUTPUT, .config = {.ospeed = ospeed_sel, \
                                                                                .otype  = GPIO_OUTPUT_PUSH_PULL } \
    }

#define GPIO_INIT_OUTPUT_OPEN_DRAIN(gpio_bank, pin_num, ospeed_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_OUTPUT, .config = {.ospeed = ospeed_sel, \
                                                                                .otype  = GPIO_OUTPUT_OPEN_DRAIN } \
    }
/**
 * @brief Create GPIO Init struct to intilize a GPIO pin for analog
 *
 * @param gpio_bank GPIO_TypeDef* reference to the GPIO bank for the pin
 * @param pin_num Pin number from GPIO bank to configure
 */
#define GPIO_INIT_ANALOG(gpio_bank, pin_num) \
    { \
        .bank = gpio_bank, \
        .pin  = pin_num, \
        .type = GPIO_TYPE_ANALOG}

/**
 * @brief Create GPIO Init struct to intilize a GPIO pin for alternate function
 *
 * @param gpio_bank GPIO_TypeDef* reference to the GPIO bank for the pin
 * @param pin_num Pin number from GPIO bank to configure
 * @param alt_func_num Alternate function selection
 * @param ospeed_sel Pin output speed selection
 * @param otype_sel Pin output type selection
 * @param input_pull_sel Input pullup/pulldown/high-z selection
 */
#define GPIO_INIT_AF(gpio_bank, pin_num, alt_func_num, ospeed_sel, otype_sel, input_pull_sel) \
    { \
        .bank = gpio_bank, .pin = pin_num, .type = GPIO_TYPE_AF, .config = {.af_num = alt_func_num, \
                                                                            .ospeed = ospeed_sel, \
                                                                            .otype  = otype_sel, \
                                                                            .pull   = input_pull_sel } \
    }

/*
    Useful defines for GPIO Init struct with commonly used peripheral/pin mappings.
    If you find yourself adding the same pin mappings to multiple devices, add a macro below
    to cut down on duplication.
*/
#ifdef STM32F407xx
//CAN
#define GPIO_INIT_CANRX_PD0 GPIO_INIT_AF(GPIOD, 0, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_CANTX_PD1 GPIO_INIT_AF(GPIOD, 1, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_CANRX_PA11 GPIO_INIT_AF(GPIOA, 11, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_CANTX_PA12 GPIO_INIT_AF(GPIOA, 12, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_CAN2RX_PB5 GPIO_INIT_AF(GPIOB, 5, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_CAN2TX_PB6 GPIO_INIT_AF(GPIOB, 6, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_CAN2RX_PB12 GPIO_INIT_AF(GPIOB, 12, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_CAN2TX_PB13 GPIO_INIT_AF(GPIOB, 13, 9, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

//SPI
#define GPIO_INIT_SPI2_SCK_PB13  GPIO_INIT_AF(GPIOB, 13, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)
#define GPIO_INIT_SPI2_MISO_PB14 GPIO_INIT_AF(GPIOB, 14, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_SPI2_MOSI_PB15 GPIO_INIT_AF(GPIOB, 15, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)

#define GPIO_INIT_SPI2_MISO_PC2 GPIO_INIT_AF(GPIOC, 2, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_SPI2_MOSI_PC3 GPIO_INIT_AF(GPIOC, 3, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)
#define GPIO_INIT_SPI1_SCK_PB3  GPIO_INIT_AF(GPIOB, 3, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)
#define GPIO_INIT_SPI2_MISO_PB4 GPIO_INIT_AF(GPIOB, 4, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_SPI1_MOSI_PB5 GPIO_INIT_AF(GPIOB, 5, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)

#define GPIO_INIT_SPI1_SCK_PA5  GPIO_INIT_AF(GPIOA, 5, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)
#define GPIO_INIT_SPI1_MISO_PA6 GPIO_INIT_AF(GPIOA, 6, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_SPI1_MOSI_PA7 GPIO_INIT_AF(GPIOA, 7, 5, GPIO_OUTPUT_HIGH_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_DOWN)

//UART/USART
#define GPIO_INIT_USART3TX_PC10 GPIO_INIT_AF(GPIOC, 10, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART3RX_PC11 GPIO_INIT_AF(GPIOC, 11, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART2TX_PA2 GPIO_INIT_AF(GPIOA, 2, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART2RX_PA3 GPIO_INIT_AF(GPIOA, 3, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART1TX_PA9  GPIO_INIT_AF(GPIOA, 9, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART1RX_PA10 GPIO_INIT_AF(GPIOA, 10, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART2TX_PD5 GPIO_INIT_AF(GPIOD, 5, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART2RX_PD6 GPIO_INIT_AF(GPIOD, 6, 7, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)

#define GPIO_INIT_USART6TX_PC6 GPIO_INIT_AF(GPIOC, 6, 8, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)
#define GPIO_INIT_USART6RX_PC7 GPIO_INIT_AF(GPIOC, 7, 8, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_OPEN_DRAIN, GPIO_INPUT_OPEN_DRAIN)
#endif

// SDIO
#define GPIO_INIT_SDIO_CLK GPIO_INIT_AF(GPIOC, 12, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)
#define GPIO_INIT_SDIO_CMD GPIO_INIT_AF(GPIOD, 2, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)
#define GPIO_INIT_SDIO_DT0 GPIO_INIT_AF(GPIOC, 8, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)
#define GPIO_INIT_SDIO_DT1 GPIO_INIT_AF(GPIOC, 9, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)
#define GPIO_INIT_SDIO_DT2 GPIO_INIT_AF(GPIOC, 10, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)
#define GPIO_INIT_SDIO_DT3 GPIO_INIT_AF(GPIOC, 11, 12, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_PULL_UP)

// MCO1 (AF0)
#define GPIO_INIT_MCO1_PA8 GPIO_INIT_AF(GPIOA, 8, 0, GPIO_OUTPUT_ULTRA_SPEED, GPIO_OUTPUT_PUSH_PULL, GPIO_INPUT_OPEN_DRAIN)

/**
 * @brief Initilize the GPIO perpheral given a list of configuration fields for all of the GPIO pins.
 *        Will also enable the GPIO RCC clock
 *
 * @param config A list of GPIOs to config
 * @param config_len Number of GPIOs in the config list
 * @return true All GPIOs were a valid configuration format
 * @return false Some of the GPIOs had an invalid configuration format
 */
bool PHAL_initGPIO(GPIOInitConfig_t config[], uint8_t config_len);

/**
 * @brief Read the state of the input register for the specific GPIO pin
 *
 * @param bank GPIO Bank of the pin
 * @param pin GPIO pin number
 * @return true GPIO Input true
 * @return false GPIO Input false
 */
static inline bool PHAL_GPIO_read(GPIO_TypeDef* bank, uint8_t pin) {
    return (bank->IDR >> pin) & 0b1;
}

/**
 * @brief Write a logic value to an output pin
 *
 * @param bank GPIO Bank of the pin
 * @param pin GPIO pin number
 * @param value Logical value to write
 */
static inline void PHAL_GPIO_write(GPIO_TypeDef* bank, uint8_t pin, bool value) {
    // BSRR has "set" as bottom 16 bits and "reset" as top 16
    bank->BSRR |= 1U << ((!value << 4) | pin);
}

static inline void PHAL_GPIO_toggle(GPIO_TypeDef* bank, uint8_t pin) {
    PHAL_GPIO_write(bank, pin, !PHAL_GPIO_read(bank, pin));
}
#endif
