/**
 * @file adc.h
 * @brief G4 ADC driver: pin-based config, continuous circular-DMA sampling.
 * @author Daniel Proano (dproano@purdue.edu) && Ronak Jain (jain717@purdue.edu)
 */

#ifndef PHAL_G4_ADC_H
#define PHAL_G4_ADC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"

static constexpr uint8_t PHAL_ADC_MAX_CHANNEL_COUNT = 16U;

/// Returned by PHAL_ADC_read() for an unconfigured pin
static constexpr uint16_t PHAL_ADC_INVALID_READING = UINT16_MAX;

/// One analog pin; build with PHAL_ADC_INIT or PHAL_ADC_INIT_ON
typedef struct {
    GPIO_TypeDef *bank;     /*!< GPIO port */
    uint8_t pin;            /*!< 0-15 */
    ADC_TypeDef *instance;  /*!< ADC1-ADC4, or nullptr to auto-assign */
} PHAL_ADC_InitConfig_t;

/**
 * @brief Set pins to analog, assign each to an ADC, and start continuous sampling
 *
 * Auto-assigned pins go to their least loaded wired ADC (ties: lowest number),
 * after forced and single-ADC pins are placed. Call once.
 *
 * @return false if already initialized, config is empty, a pin is unmapped or
 *         duplicated, a forced ADC isn't wired to its pin, or hardware setup fails
 */
bool PHAL_ADC_init(const PHAL_ADC_InitConfig_t config[], size_t config_len);

/**
 * @brief Latest 12-bit sample (0-4095) for a pin; safe from any context
 * @return PHAL_ADC_INVALID_READING if the pin wasn't configured
 */
uint16_t PHAL_ADC_read(const GPIO_TypeDef *bank, uint8_t pin);

/// Analog pin, ADC auto-assigned
#define PHAL_ADC_INIT(gpio_bank, pin_num)                                      \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .instance = nullptr                                                    \
    }

/// Analog pin on a specific ADC, which must be wired to the pin
#define PHAL_ADC_INIT_ON(gpio_bank, pin_num, adc_instance)                     \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .instance = adc_instance                                               \
    }

#endif // PHAL_G4_ADC_H
