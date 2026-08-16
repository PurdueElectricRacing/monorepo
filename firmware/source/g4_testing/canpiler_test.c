#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_CANPILER)

#include <stdint.h>

#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/freertos/freertos.h"
#include "common/utils/countof.h"
#include "can_library/can_common.h"
#include "can_library/generated/G4_TESTING.h"


GPIOInitConfig_t gpio_config[] = {
    GPIO_INIT_FDCAN2RX_PB12,
    GPIO_INIT_FDCAN2TX_PB13
};

void HardFault_Handler();

/** 
 * @brief Encode and transmit canpiler_test message
 *
 * Sends canpiler_test with fixed physical values, encoded using the
 * generated constants. Daqapp decodes via the DBC and should show the PHYS
 * values below. physical = raw * scale + offset
 *
 * signal       encode                        PHYS(shown by daqapp)     raw     
 * temperature  phys - OFFSET                  25.0 C                   65, -40C offset  
 * current      phys * PACK_COEFF             -125.5 A                 -1255, 0.1A scale
 * voltage      (phys - OFFSET) * PACK_COEFF   3.700 V                  1200, 0.001V scale, 2.5V offset   
 * pressure     (phys - OFFSET) * PACK_COEFF   300.0 kPa                140, 2.0 kPa scale, 20kPa offset
 * status       phys (no scale/offset)         165                      165, no scale/offset
 */
void send_periodic() {
    uint8_t temperature = (uint8_t)(25.0f - OFFSET_CANPILER_TEST_TEMPERATURE);
    int16_t current = (int16_t)(-125.5f * PACK_COEFF_CANPILER_TEST_CURRENT);
    uint16_t voltage = (uint16_t)((3.700f - OFFSET_CANPILER_TEST_VOLTAGE) * PACK_COEFF_CANPILER_TEST_VOLTAGE);
    uint8_t pressure = (uint8_t)((300.0f - OFFSET_CANPILER_TEST_PRESSURE) * PACK_COEFF_CANPILER_TEST_PRESSURE);
    uint8_t status = 165;

    CAN_SEND_canpiler_test(temperature, current, voltage, pressure, status);
}

DEFINE_CAN_TASKS();
FREERTOS_DEFINE_TASK(send_periodic, 100, TASK_PRIORITY_NORMAL, 512);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN2, GCAN_BAUD_RATE);
    CAN_init();

    START_CAN_TASKS();  
    FREERTOS_START_TASK(send_periodic);

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_CANPILER
