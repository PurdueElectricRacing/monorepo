#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_BUSOFF)

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "main.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "can_library/can_common.h"
#include "can_library/generated/G4_TESTING.h"

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

// LED_GREEN_PORT, LED_GREEN_PIN

PHAL_GPIO_InitConfig_t gpio_config[] = {
    // PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12
};

void increase_error_counter() {
    
}

void get_error_counter() {
    
}

DEFINE_CAN_TASKS(get_error_counter, 250, TASK_PRIORITY_NORMAL, 256);
// RTOS_DEFINE_TASK(ledblinkk, 250, TASK_PRIORITY_NORMAL, 64);
int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN1, TEST_CAN_BAUD_RATE);
    CAN_init();

    START_CAN_TASKS();
    RTOS_START_TASK(get_error_counter);

    CAN_send_error_counter(0xDEADBEEF);

    vTaskStartScheduler();

    return 0;
}


#endif // G4_TESTING_CHOSEN == TEST_BUSOFF



/*

#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_BUSOFF)

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "main.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "can_library/can_common.h"
#include "can_library/generated/G4_TESTING.h"

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

// LED_GREEN_PORT, LED_GREEN_PIN

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    //PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    //PHAL_PIN_DEFS_FDCAN1_TX_PA12
};

static void ledblinkk(void) {
    PHAL_GPIO_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
}


void increase_error_counter() {
    
}

void get_error_counter() {
    
}

RTOS_DEFINE_TASK(ledblinkk, 250, TASK_PRIORITY_NORMAL, 64);
int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, 1);
    RTOS_START_TASK(ledblinkk);

    vTaskStartScheduler();

    return 0;
}


#endif // G4_TESTING_CHOSEN == TEST_BUSOFF


*/

