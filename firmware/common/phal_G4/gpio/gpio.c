/**
 * @file gpio.c
 * @brief G4 GPIO public API implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/gpio/gpio.h"

#include "common/phal_G4/gpio/gpio_priv.h"

bool PHAL_GPIO_init(PHAL_GPIO_InitConfig_t config[], size_t config_len) {
    for (size_t i = 0; i < config_len; i++) {
        GPIO_TypeDef *bank = config[i].bank;
        uint8_t pin        = config[i].pin;

        if (pin > 15U) {
            return false;
        }
        if (!PHAL_GPIO_priv_enableClock(bank)) {
            return false;
        }

        switch (config[i].type) {
            case GPIO_TYPE_INPUT:
                PHAL_GPIO_priv_setPull(bank, pin, config[i].config.input.pull);
                PHAL_GPIO_priv_setMode(bank, pin, GPIO_TYPE_INPUT);
                break;

            case GPIO_TYPE_OUTPUT:
                PHAL_GPIO_priv_setOutputSpeed(bank, pin, config[i].config.output.ospeed);
                PHAL_GPIO_priv_setOutputType(bank, pin, config[i].config.output.otype);
                PHAL_GPIO_priv_setMode(bank, pin, GPIO_TYPE_OUTPUT);
                break;

            case GPIO_TYPE_AF:
                PHAL_GPIO_priv_setAltFunction(bank, pin, config[i].config.af.af_num);
                PHAL_GPIO_priv_setOutputSpeed(bank, pin, config[i].config.af.ospeed);
                PHAL_GPIO_priv_setOutputType(bank, pin, config[i].config.af.otype);
                PHAL_GPIO_priv_setPull(bank, pin, config[i].config.af.pull);
                PHAL_GPIO_priv_setMode(bank, pin, GPIO_TYPE_AF);
                break;

            case GPIO_TYPE_ANALOG:
                PHAL_GPIO_priv_setPull(bank, pin, GPIO_INPUT_OPEN_DRAIN);
                PHAL_GPIO_priv_setMode(bank, pin, GPIO_TYPE_ANALOG);
                break;

            default:
                return false;
        }
    }

    return true;
}