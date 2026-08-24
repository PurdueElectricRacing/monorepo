#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_FLASH)

#include <stdint.h>

#include "common/phal_G4/flash/flash.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

void HardFault_Handler();

#define FLASH_TEST_PAGE (FLASH_BASE + (508U * 1024U))

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

static const uint8_t g_flash_patterns[][13] = {
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x0F, 0xED, 0xCB, 0xA9, 0x87},
    {0xA5, 0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB},
    {0xDE, 0xAD, 0xBE, 0xEF, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90},
    {0xF0, 0x0D, 0xCA, 0xFE, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x5A},
};
static uint8_t g_flash_copy[sizeof(g_flash_patterns[0])] = {0};
static uint8_t g_flash_readback[sizeof(g_flash_patterns[0])] = {0};

static bool flash_copy(uint32_t source_address, uint32_t destination_address, uint32_t length_bytes) {
    if (!PHAL_FLASH_read(source_address, g_flash_copy, length_bytes)) {
        return false;
    }
    if (!PHAL_FLASH_erase(destination_address, length_bytes)) {
        return false;
    }
    if (!PHAL_FLASH_write(destination_address, g_flash_copy, length_bytes)) {
        return false;
    }
    if (!PHAL_FLASH_read(destination_address, g_flash_readback, length_bytes)) {
        return false;
    }

    for (uint32_t i = 0U; i < length_bytes; i++) {
        if (g_flash_copy[i] != g_flash_readback[i]) {
            return false;
        }
    }

    return true;
} 

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);


    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    bool match = true;
    for (uint32_t cycle = 0U; cycle < countof(g_flash_patterns); cycle++) {
        uint32_t source_address = (uint32_t)(uintptr_t)g_flash_patterns[cycle];
        match = flash_copy(source_address, FLASH_TEST_PAGE, sizeof(g_flash_patterns[0])) && match;
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, match ? 1 : 0);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, match ? 0 : 1);

    return 0;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_FLASH
