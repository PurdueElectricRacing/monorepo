#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_DEARUNITS)

#include <stdint.h>

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "dearunits_library/generated/dear_units.h"
#include "main.h"

void HardFault_Handler(void);

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

// Left in place after each run so they're inspectable in a debugger.
volatile float dearunits_mile_in_meters   = 0.0f;
volatile float dearunits_velocity_mps     = 0.0f;
volatile float dearunits_velocity_mph     = 0.0f;
volatile float dearunits_force_newtons    = 0.0f;
volatile float dearunits_torque_nm        = 0.0f;

static bool nearly_equal(float a, float b, float tolerance) {
    float diff = (a > b) ? (a - b) : (b - a);
    return diff <= tolerance;
}

static bool run_dearunits_test(void) {
    mile_t one_mile = { .value = 1.0f };
    meter_t mile_in_meters = meter_from(one_mile);
    dearunits_mile_in_meters = mile_in_meters.value;
    if (!nearly_equal(mile_in_meters.value, 1609.34f, 0.01f)) {
        return false;
    }

    meter_t distance = { .value = 100.0f };
    second_t time = { .value = 10.0f };
    meters_per_second_t velocity = velocity_from(distance, time);
    dearunits_velocity_mps = velocity.value;
    if (!nearly_equal(velocity.value, 10.0f, 0.001f)) {
        return false;
    }

    miles_per_hour_t velocity_mph = miles_per_hour_from_meters_per_second(velocity);
    dearunits_velocity_mph = velocity_mph.value;
    if (!nearly_equal(velocity_mph.value, 22.3694f, 0.01f)) {
        return false;
    }

    // A compound type built from base-class values (force = mass * length / time^2)...
    kilogram_t mass = { .value = 10.0f };
    meter_t arm = { .value = 2.0f };
    second_t interval = { .value = 1.0f };
    newton_t force = force_from(mass, arm, interval);
    dearunits_force_newtons = force.value;
    if (!nearly_equal(force.value, 20.0f, 0.001f)) {
        return false;
    }

    newton_meter_t torque = torque_from(force, arm);
    dearunits_torque_nm = torque.value;
    if (!nearly_equal(torque.value, 40.0f, 0.001f)) {
        return false;
    }

    return true;
}

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    bool pass = run_dearunits_test();

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

#endif // G4_TESTING_CHOSEN == TEST_DEARUNITS
