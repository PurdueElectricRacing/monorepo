#include "activity_leds.h"

#include "phal_G4/gpio/gpio.h"
#include "main.h"

static volatile uint32_t g_last_activity[ACTIVITY_COUNT];
static volatile bool g_activity_seen[ACTIVITY_COUNT];

void activity_leds_record(activity_led_t led, uint32_t tick) {
    g_last_activity[led] = tick;
    g_activity_seen[led] = true;
}

void activity_leds_periodic(void) {
    bool set_on[ACTIVITY_COUNT] = {false};

    // Snapshot timestamps and time together so ISR can't trigger a timestamp newer than now
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    uint32_t now = xTaskGetTickCount();
    for (size_t i = 0U; i < ACTIVITY_COUNT; ++i) {
        uint32_t age = (uint32_t)(now - g_last_activity[i]);
        bool is_recent = age < ACTIVITY_LED_TIMEOUT_MS;
        set_on[i] = g_activity_seen[i] && is_recent;
    }
    __set_PRIMASK(saved_primask);

    PHAL_GPIO_write(VCAN_LED_PORT, VCAN_LED_PIN, set_on[ACTIVITY_VCAN]);
    PHAL_GPIO_write(MCAN_LED_PORT, MCAN_LED_PIN, set_on[ACTIVITY_MCAN]);
    PHAL_GPIO_write(SCAN_LED_PORT, SCAN_LED_PIN, set_on[ACTIVITY_SCAN]);
    PHAL_GPIO_write(USB_LED_PORT,  USB_LED_PIN,  set_on[ACTIVITY_USB]);
}
