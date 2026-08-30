/**
 * @file heartbeat.c
 * @brief Shared heartbeat task implementation for status LEDs.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "heartbeat.h"
#include "common/rtos/rtos.h"

extern volatile uint32_t last_can_rx_time_ms;

static void preflight_led_sweep(status_leds_t *leds) {
    static uint32_t sweep_index = 0;

    switch (sweep_index++ % 3) {
        case 0:
            PHAL_GPIO_write(leds->heartbeat_port, leds->heartbeat_pin, 1);
            PHAL_GPIO_write(leds->connection_port, leds->connection_pin, 0);
            PHAL_GPIO_write(leds->error_port, leds->error_pin, 0);
            break;
        case 1:
            PHAL_GPIO_write(leds->heartbeat_port, leds->heartbeat_pin, 0);
            PHAL_GPIO_write(leds->connection_port, leds->connection_pin, 1);
            PHAL_GPIO_write(leds->error_port, leds->error_pin, 0);
            break;
        case 2:
            PHAL_GPIO_write(leds->heartbeat_port, leds->heartbeat_pin, 0);
            PHAL_GPIO_write(leds->connection_port, leds->connection_pin, 0);
            PHAL_GPIO_write(leds->error_port, leds->error_pin, 1);
            break;
    }
}

void heartbeat_task(status_leds_t *leds) {
    switch (leds->state) {
        case HEARTBEAT_STATE_PREFLIGHT:
            preflight_led_sweep(leds);

            if (leds->preflight_callback) {
                leds->preflight_callback();
            }

            // exit preflight after a certain time has elapsed
            if (xTaskGetTickCount() > PREFLIGHT_DURATION_MS) {
                PHAL_GPIO_write(leds->heartbeat_port, leds->heartbeat_pin, 0);
                PHAL_GPIO_write(leds->connection_port, leds->connection_pin, 0);
                PHAL_GPIO_write(leds->error_port, leds->error_pin, 0);
                leds->state = HEARTBEAT_STATE_NORMAL;
            }
            break;
        case HEARTBEAT_STATE_NORMAL:
            PHAL_GPIO_toggle(leds->heartbeat_port, leds->heartbeat_pin);

            bool is_can_ok = (xTaskGetTickCount() - last_can_rx_time_ms) < CONN_LED_TIMEOUT_MS;
            PHAL_GPIO_write(leds->connection_port, leds->connection_pin, is_can_ok);
            break;
    }    
}
