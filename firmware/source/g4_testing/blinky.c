#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_BLINKY)

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_OUTPUT_LOW_SPEED),
};

void HardFault_Handler();

static void ledblink1(void);
static void ledblink2(void);
static void ledblink3(void);
static void ledblink4(void);

RTOS_DEFINE_TASK(ledblink1, 250, TASK_PRIORITY_NORMAL, 64);
RTOS_DEFINE_TASK(ledblink2, 300, TASK_PRIORITY_NORMAL, 64);
RTOS_DEFINE_TASK(ledblink3, 500, TASK_PRIORITY_NORMAL, 64);
RTOS_DEFINE_TASK(ledblink4, 1000, TASK_PRIORITY_NORMAL, 64);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, 1);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, 1);
    PHAL_GPIO_write(LED_BLUE_PORT, LED_BLUE_PIN, 1);
    PHAL_GPIO_write(LED_ORANGE_PORT, LED_ORANGE_PIN, 1);

    // Create threads
    RTOS_START_TASK(ledblink1);
    RTOS_START_TASK(ledblink2);
    RTOS_START_TASK(ledblink3);
    RTOS_START_TASK(ledblink4);

    vTaskStartScheduler();

    return 0;
}

static void ledblink1(void) {
    PHAL_GPIO_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
}

static void ledblink2(void) {
    PHAL_GPIO_toggle(LED_RED_PORT, LED_RED_PIN);
}

static void ledblink3(void) {
    PHAL_GPIO_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
}

static void ledblink4(void) {
    PHAL_GPIO_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_BLINKY
