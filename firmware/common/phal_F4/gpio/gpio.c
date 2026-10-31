#include "common/phal_F4/gpio/gpio.h"

bool PHAL_initGPIO(GPIOInitConfig_t config[], uint8_t config_len) {
    uint8_t afr_i;

    for (int i = 0; i < config_len; i++) {
        // Enable clock
        // Not sure if this should live in the GPIO init or a separate RCC HAL lib
        switch ((uint32_t)config[i].bank) {
            case (uint32_t)GPIOA:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOB:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOC:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOD:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOE:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOF:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOG:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            case (uint32_t)GPIOH:
                RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;
                (void)RCC->AHB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
                break;

            default:
                return false;
                break;
        }

        // Setup GPIO output type
        config[i].bank->MODER &= ~(GPIO_MODER_MODER0_Msk << (GPIO_MODER_MODER1_Pos * config[i].pin));
        config[i].bank->MODER |= (config[i].type & GPIO_MODER_MODER0_Msk) << (GPIO_MODER_MODER1_Pos * config[i].pin);

        switch (config[i].type) {
            case GPIO_TYPE_OUTPUT:
                config[i].bank->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0_Msk << (GPIO_OSPEEDR_OSPEED1_Pos * config[i].pin));
                config[i].bank->OSPEEDR |= (config[i].config.ospeed & GPIO_OSPEEDR_OSPEED0_Msk) << (GPIO_OSPEEDR_OSPEED1_Pos * config[i].pin);

                config[i].bank->OTYPER &= ~(GPIO_OTYPER_OT0_Msk << (GPIO_OTYPER_OT1_Pos * config[i].pin));
                config[i].bank->OTYPER |= (config[i].config.otype & GPIO_OTYPER_OT0_Msk) << (GPIO_OTYPER_OT1_Pos * config[i].pin);
                break;

            case GPIO_TYPE_INPUT:
                config[i].bank->PUPDR &= ~(GPIO_PUPDR_PUPD0_Msk << (GPIO_PUPDR_PUPD1_Pos * config[i].pin));
                config[i].bank->PUPDR |= (config[i].config.pull & GPIO_PUPDR_PUPD0_Msk) << (GPIO_PUPDR_PUPD1_Pos * config[i].pin);
                break;

            case GPIO_TYPE_AF:
                afr_i = config[i].pin > 7 ? 1 : 0;
                config[i].bank->AFR[afr_i] &= ~(GPIO_AFRL_AFSEL0_Msk << (GPIO_AFRL_AFSEL1_Pos * (config[i].pin % 8)));
                config[i].bank->AFR[afr_i] |= (config[i].config.af_num & GPIO_AFRL_AFSEL0_Msk) << (GPIO_AFRL_AFSEL1_Pos * (config[i].pin % 8));

                config[i].bank->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0_Msk << (GPIO_OSPEEDR_OSPEED1_Pos * config[i].pin));
                config[i].bank->OSPEEDR |= (config[i].config.ospeed & GPIO_OSPEEDR_OSPEED0_Msk) << (GPIO_OSPEEDR_OSPEED1_Pos * config[i].pin);

                config[i].bank->OTYPER &= ~(GPIO_OTYPER_OT0_Msk << (GPIO_OTYPER_OT1_Pos * config[i].pin));
                config[i].bank->OTYPER |= (config[i].config.otype & GPIO_OTYPER_OT0_Msk) << (GPIO_OTYPER_OT1_Pos * config[i].pin);

                config[i].bank->PUPDR &= ~(GPIO_PUPDR_PUPD0_Msk << (GPIO_PUPDR_PUPD1_Pos * config[i].pin));
                config[i].bank->PUPDR |= (config[i].config.pull & GPIO_PUPDR_PUPD0_Msk) << (GPIO_PUPDR_PUPD1_Pos * config[i].pin);
                break;

            case GPIO_TYPE_ANALOG:
                break;

            default:
                return false;
        }
    }

    return true;
}
