#ifndef HEARTBEAT_H
#define HEARTBEAT_H

/**
 * @file heartbeat.h
 * @brief Shared heartbeat task implementation for status LEDs.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#if defined(STM32F407xx)
#include "common/phal_F4/gpio/gpio.h"
#elif defined(STM32G474xx)
#include "common/phal_G4/gpio/gpio.h"
#else
#error "Unsupported heartbeat target"
#endif
#include "common/rtos/rtos.h"

static constexpr uint32_t PREFLIGHT_DURATION_MS = 1500;
static constexpr uint32_t HEARTBEAT_PERIOD_MS = 100;
static constexpr uint32_t CONN_LED_TIMEOUT_MS = 1000;

typedef enum : uint8_t {
    HEARTBEAT_STATE_PREFLIGHT,
    HEARTBEAT_STATE_NORMAL
} heartbeat_state_t;

typedef struct {
    GPIO_TypeDef *heartbeat_port;
    uint8_t heartbeat_pin;
    GPIO_TypeDef *connection_port;
    uint8_t connection_pin;
    GPIO_TypeDef *error_port;
    uint8_t error_pin;
    void (*preflight_callback)(void);
    heartbeat_state_t state;
} status_leds_t;

extern void heartbeat_task(status_leds_t *leds);

/**
 * @brief Macro to define a heartbeat task with predefined behavior for status LEDs.
 * 
 * Usage:
 * 1. Ensure that the macros HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, CONNECTION_LED_PORT, CONNECTION_LED_PIN, ERROR_LED_PORT, and ERROR_LED_PIN are defined
 * 2. Init the GPIOS for the LEDs as outputs.
 * 3. Call DEFINE_HEARTBEAT_TASK() in your source file to define the task.
 * 4. Call START_HEARTBEAT_TASK() in your main function after initializing the kernel to start the task.
 *
 * @param PREFLIGHT_CALLBACK optional callback function periodically called during the preflight state.
 */
#define DEFINE_HEARTBEAT_TASK(PREFLIGHT_CALLBACK) \
    status_leds_t status_leds = { \
        .heartbeat_port = HEARTBEAT_LED_PORT, \
        .heartbeat_pin = HEARTBEAT_LED_PIN, \
        .connection_port = CONNECTION_LED_PORT, \
        .connection_pin = CONNECTION_LED_PIN, \
        .error_port = ERROR_LED_PORT, \
        .error_pin = ERROR_LED_PIN, \
        .preflight_callback = (PREFLIGHT_CALLBACK), \
        .state = HEARTBEAT_STATE_PREFLIGHT \
    }; \
    void heartbeat_wrapper(void) { heartbeat_task(&status_leds); }; \
    RTOS_DEFINE_TASK(heartbeat_wrapper, HEARTBEAT_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512)

#define START_HEARTBEAT_TASK() RTOS_START_TASK(heartbeat_wrapper)

#endif // HEARTBEAT_H
