#include "g4_testing.h"
#if defined(BOOTLOADER_ENABLED)

#include "can_library/generated/MAIN_MODULE.h"
#include "common/bootloader/application_version.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "main.h"

#define TEST_APP_VERSION_PERIOD_MS 5000U

static void blink_nucleo_led(void);
static void report_version(void);

static PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
    PHAL_GPIO_INIT_OUTPUT(LED_NUCLEO_GREEN_PORT, LED_NUCLEO_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
};

RTOS_DEFINE_TASK(blink_nucleo_led, 500, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(report_version, TEST_APP_VERSION_PERIOD_MS, TASK_PRIORITY_LOW, STACK_256);
DEFINE_CAN_TASKS();

void HardFault_Handler(void);

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    CAN_init();
    START_CAN_TASKS();
    RTOS_START_TASK(blink_nucleo_led);
    RTOS_START_TASK(report_version);

    vTaskStartScheduler();
    return 0;
}

static void blink_nucleo_led(void) {
    PHAL_GPIO_toggle(LED_NUCLEO_GREEN_PORT, LED_NUCLEO_GREEN_PIN);
}

static void report_version(void) {
    CAN_SEND_main_version(GIT_HASH, APPLICATION_BOOTLOADABLE);
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // defined(BOOTLOADER_ENABLED)
