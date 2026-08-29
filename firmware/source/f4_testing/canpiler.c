#include "f4_testing.h"
#if (F4_TESTING_CHOSEN == TEST_CANPILER)

#include "can_library/generated/PDU.h"
#include "common/phal_F4/adc/adc.h"
#include "common/phal_F4/can/can.h"
#include "common/phal_F4/dma/dma.h"
#include "common/phal_F4/gpio/gpio.h"
#include "common/phal_F4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"

GPIOInitConfig_t gpio_config[] = {
    // CAN
    GPIO_INIT_CANRX_PD0,
    GPIO_INIT_CANTX_PD1
};

#define TargetCoreClockrateHz 16'000'000
ClockRateConfig_t clock_config = {
    .clock_source              = CLOCK_SOURCE_HSI,
    .use_pll                   = false,
    .vco_output_rate_target_hz = 160'000'000,
    .system_clock_target_hz    = TargetCoreClockrateHz,
    .ahb_clock_target_hz       = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz      = (TargetCoreClockrateHz / (1)),
    .apb2_clock_target_hz      = (TargetCoreClockrateHz / (1)),
};

extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;
extern uint32_t AHBClockRateHz;
extern uint32_t PLLClockRateHz;

void HardFault_Handler();

void send_periodic() {
    PHAL_GPIO_toggle(GPIOC, 15);
    CAN_SEND_pdu_version(GIT_HASH);
}

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(send_periodic, 100, TASK_PRIORITY_NORMAL, 1024);

int main() {
    if (PHAL_configureClockRates(&clock_config)) {
        HardFault_Handler();
    }

    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    if (!PHAL_initCAN(CAN1, false, VCAN_BAUD_RATE)) {
        HardFault_Handler();
    }
    CAN_init();

    START_CAN_TASKS();
    RTOS_START_TASK(send_periodic);
    
    vTaskStartScheduler();

    return 0;
}

// just to avoid linker error
void cooling_driver_request_CALLBACK() {
    (void)can_data; // unused
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // F4_TESTING_CHOSEN == TEST_CANPILER
