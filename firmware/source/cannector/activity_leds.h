#ifndef CANNECTOR_ACTIVITY_LEDS_H
#define CANNECTOR_ACTIVITY_LEDS_H

#include "rtos/rtos.h"

static constexpr uint32_t ACTIVITY_LED_TIMEOUT_MS = 100U;

typedef enum {
    ACTIVITY_VCAN,
    ACTIVITY_MCAN,
    ACTIVITY_SCAN,
    ACTIVITY_USB,
    ACTIVITY_COUNT,
} activity_led_t;

// Safe to call from both a task and ISR
void activity_leds_record(activity_led_t led, TickType_t tick);

// Periodically update the activity LEDs based on recent activity
void activity_leds_periodic(void);

#endif
