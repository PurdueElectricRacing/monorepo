#include "g4_testing.h"
#include "stm32g474xx.h"
#if (G4_TESTING_CHOSEN == TEST_CRC)

#include <stdint.h>

#include "common/phal_G4/crc/crc.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

void HardFault_Handler(void);

static constexpr uint32_t TargetCoreClockrateHz = 16'000'000;
ClockRateConfig_t clock_config = {
    .clock_source           = CLOCK_SOURCE_HSI,
    .system_clock_target_hz = TargetCoreClockrateHz,
    .ahb_clock_target_hz    = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz   = (TargetCoreClockrateHz / (1)),
    .apb2_clock_target_hz   = (TargetCoreClockrateHz / (1)),
};

GPIOInitConfig_t gpio_config[] = {
    GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

typedef struct {
    const uint32_t *data;
    uint32_t words;
    uint32_t expected;
} crc_vector_t;

static const uint32_t v_zero[] = {0x00000000};
static const uint32_t v_ff[] = {0xFFFFFFFF};
static const uint32_t v_single[] = {0x12345678};
static const uint32_t v_highbyte[] = {0xAABBCCDD};
static const uint32_t v_two[] = {0x12345678, 0x9ABCDEF0};
static const uint32_t v_four[]= {0x12345678, 0x9ABCDEF0, 0xDEADBEEF, 0xCAFEBABE};

volatile uint32_t crc_failed_index = -1;
volatile uint32_t crc_failed_hw = 0;
volatile uint32_t crc_failed_sw = 0;
volatile uint32_t crc_failed_expected = 0;

static bool run_crc_test(void) {
    static const crc_vector_t vectors[] = {
        {v_zero, countof(v_zero), 0xC704DD7B},
        {v_ff, countof(v_ff), 0x00000000},
        {v_single, countof(v_single), 0xDF8A8A2B},
        {v_highbyte, countof(v_highbyte), 0x246E87F0},
        {v_two, countof(v_two), 0x7D24A31B},
        {v_four, countof(v_four), 0xF53DA296},
    };

    for (uint32_t i = 0; i < countof(vectors); i++) {
        const crc_vector_t *v = &vectors[i];

        uint32_t hw = PHAL_CRC_calculate(v->data, v->words);
        uint32_t sw = PHAL_CRC_calculateSw(v->data, v->words);

        if (hw != v->expected || sw != v->expected) {
            crc_failed_index = i;
            crc_failed_hw = hw;
            crc_failed_sw = sw;
            crc_failed_expected = v->expected;
            return false;
        }
    }
    return true;
}

int main(void) {
    if (PHAL_configureClockRates(&clock_config)) {
        HardFault_Handler();
    }

    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_CRC_init();

   bool pass = run_crc_test();

   PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, pass ? 1 : 0);
   PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, pass ? 0 : 1);

    while (1) {
        __asm__("nop");
    }
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_CRC