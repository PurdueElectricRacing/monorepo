/**
 * @file main.c
 * @brief "Torque Vector" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/generated/TORQUE_VECTOR.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usart/usart.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"

/* Module Includes */
#include "control_loop.h"
#include "sensors.h"
#include "telemetry.h"

/* PER HAL Initialization Structures */
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Status LEDs
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // VCAN
    PHAL_PIN_DEFS_FDCAN2_TX_PB13, // we fly swapped TX/RX
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,

    // GCAN
    // ! these pin are erroneously swapped on the schematic
    // GPIO_INIT_FDCAN1TX_PA12,
    // PHAL_PIN_DEFS_FDCAN2_TX_PB13,

    // Rover GPS
    PHAL_PIN_DEFS_USART3_RX_PB11,
    PHAL_PIN_DEFS_USART3_TX_PB10,
    PHAL_GPIO_INIT_OUTPUT(ROVER_RESET_PORT, ROVER_RESET_PIN, GPIO_OUTPUT_LOW_SPEED),

    // Base GPS
    PHAL_PIN_DEFS_USART1_RX_PA10,
    PHAL_PIN_DEFS_USART1_TX_PA9,
    PHAL_GPIO_INIT_OUTPUT(BASE_RESET_PORT, BASE_RESET_PIN, GPIO_OUTPUT_LOW_SPEED),
};

// USART Configuration for GPS
static constexpr uint32_t GPS_BAUD_RATE = 460'800;
static constexpr PHAL_USART_Idx_t GPS_USART = USART3_IDX;

extern void HardFault_Handler(void);

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(control_loop, CONTROL_LOOP_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_4096);
RTOS_DEFINE_TASK(gps_periodic, GPS_THREAD_PERIOD_MS, TASK_PRIORITY_LOW, STACK_1024);
RTOS_DEFINE_TASK(report_telemetry_25hz, TELEMETRY_25HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_1hz, TELEMETRY_1HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main(void) {
    // Hardware Initialization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }
    if (false == PHAL_USART_init(GPS_USART, GPS_BAUD_RATE, PHAL_RCC_getAPB1ClockHz())) {
        HardFault_Handler();
    }
    PHAL_USART_rx(GPS_USART, (uint8_t *)rover_rx_buffer, sizeof(rover_rx_buffer), true);
    PHAL_FDCAN_init(FDCAN2, VCAN_BAUD_RATE);
    CAN_init();

    initialize_calibration();

    PHAL_GPIO_write(ROVER_RESET_PORT, ROVER_RESET_PIN, 1);
    PHAL_GPIO_write(BASE_RESET_PORT, BASE_RESET_PIN, 1);

    control_init();

    // Software Initialization
    START_CAN_TASKS();
    CAN_SEND_tv_init(WDG_get_CSR());
    RTOS_START_TASK(control_loop);
    RTOS_START_TASK(gps_periodic);
    RTOS_START_TASK(report_telemetry_25hz);
    RTOS_START_TASK(report_telemetry_1hz);
    START_HEARTBEAT_TASK();
    START_WATCHDOG_TASK();

    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL        = 0;
    ERROR_LED_PORT->BSRR = (1 << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}

