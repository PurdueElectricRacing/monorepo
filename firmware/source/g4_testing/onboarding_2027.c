#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_ONBOARDING_2027)

#include <stdint.h>

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"


// Hmmm... something may go here


PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Hmmm... something may go here
};

void HardFault_Handler();

// Hmmm... something(s) may go here

int main() {
    // NUCLEO-G474RE does not have an external oscillator so use the internal 16MHz HSI clock
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Hmmm... something(s) may go here

    return 0;
}


void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_ONBOARDING_2027
