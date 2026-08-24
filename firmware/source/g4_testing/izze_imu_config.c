#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == IZZE_IMU_CONFIG)

#include "can_library/generated/G4_TESTING.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "common/izze_imu/izze_imu.h"
#include "common/utils/countof.h"

// Status LEDs
#define HEARTBEAT_LED_PORT  (GPIOB)
#define HEARTBEAT_LED_PIN   (4)
#define ERROR_LED_PORT      (GPIOA)
#define ERROR_LED_PIN       (15)
#define CONNECTION_LED_PORT (GPIOB)
#define CONNECTION_LED_PIN  (3)

GPIOInitConfig_t gpio_config[] = {
    GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_FDCAN2RX_PB12,
    GPIO_INIT_FDCAN2TX_PB13
};

void HardFault_Handler();

static constexpr uint16_t NEW_CAN_BASE_ID = 0x4EE;
static_assert(NEW_CAN_BASE_ID >= 1, "CAN Base ID must be greater than or equal to 1");
static_assert(NEW_CAN_BASE_ID <= 0x7FF, "CAN Base ID must be less than or equal to 0x7FF");
// todo more assertions?

static constexpr uint32_t IMU_CONFIG_TIME_MS = 12'000; // "at least 10 seconds"
void config_imu() {
    if (xTaskGetTickCount() >= IMU_CONFIG_TIME_MS) {
        // set LED
        PHAL_GPIO_write(CONNECTION_LED_PORT, CONNECTION_LED_PIN, 1);
        // Delete task
        vTaskDelete(NULL);
    }

    PHAL_GPIO_toggle(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN);
    CAN_SEND_IZZE_IMU_config(
        IZZE_IMU_PROGRAMMING_CONSTANT,
        NEW_CAN_BASE_ID,
        GYRO_SCALE_250DPS, ACCEL_SCALE_2G, ODR_104HZ, BIT_RATE_1MBPS
    );
}

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(config_imu, IZZE_IMU_CONFIG_PERIOD_MS, TASK_PRIORITY_NORMAL, 1024);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_initGPIO(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, 0);
    PHAL_GPIO_write(ERROR_LED_PORT, ERROR_LED_PIN, 0);
    PHAL_GPIO_write(CONNECTION_LED_PORT, CONNECTION_LED_PIN, 0);

    PHAL_FDCAN_init(FDCAN2, GCAN_BAUD_RATE);

    CAN_init();

    START_CAN_TASKS();
    RTOS_START_TASK(config_imu);

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


#endif // G4_TESTING_CHOSEN == IZZE_IMU_CONFIG
