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
    if (!nearly_equal(mile_in_meters.value, 1609.344f, 0.001f)) {
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

    // Multiply/divide resolve result types from the unit dimensions...
    meter_t travelled = DEARUNITS_MULTIPLY(velocity, time);
    if (!nearly_equal(travelled.value, 100.0f, 0.001f)) {
        return false;
    }

    second_t travel_time = DEARUNITS_DIVIDE(distance, velocity);
    if (!nearly_equal(travel_time.value, 10.0f, 0.001f)) {
        return false;
    }

    float distance_ratio = DEARUNITS_DIVIDE(distance, travelled);
    if (!nearly_equal(distance_ratio, 1.0f, 0.001f)) {
        return false;
    }

    meter_t half_distance = DEARUNITS_DIVIDE(distance, 2.0f);
    if (!nearly_equal(half_distance.value, 50.0f, 0.001f)) {
        return false;
    }

    // ...and same-type helpers work on any unit type.
    meter_t clamped = DEARUNITS_CLAMP(distance, arm, half_distance);
    if (!nearly_equal(clamped.value, 50.0f, 0.001f)) {
        return false;
    }

    if (!DEARUNITS_LT(arm, distance) || DEARUNITS_GT(arm, distance)) {
        return false;
    }

    meter_t gap = DEARUNITS_ABS_DIFF(arm, distance);
    if (!nearly_equal(gap.value, 98.0f, 0.001f)) {
        return false;
    }

    // Electrical/energy types resolve through dimensional analysis and explicit relations.
    volt_t pack_voltage = { .value = 400.0f };
    amp_t pack_current = { .value = 50.0f };
    watt_t pack_power = DEARUNITS_MULTIPLY(pack_voltage, pack_current);
    if (!nearly_equal(pack_power.value, 20000.0f, 0.01f)) {
        return false;
    }

    joule_t pack_energy = DEARUNITS_MULTIPLY(pack_power, (second_t){ .value = 3600.0f });
    kilowatt_hour_t pack_kwh = kilowatt_hour_from_joule(pack_energy);
    if (!nearly_equal(pack_kwh.value, 20.0f, 0.001f)) {
        return false;
    }

    ohm_t pack_resistance = DEARUNITS_DIVIDE(pack_voltage, pack_current);
    if (!nearly_equal(pack_resistance.value, 8.0f, 0.001f)) {
        return false;
    }

    // Mechanical power = torque * angular velocity
    radians_per_second_t motor_speed = { .value = 100.0f };
    watt_t mech_power = DEARUNITS_MULTIPLY(torque, motor_speed);
    if (!nearly_equal(mech_power.value, 4000.0f, 0.01f)) {
        return false;
    }

    // Squares, roots, and scalar-first
    square_meter_t area = DEARUNITS_SQUARE(arm);
    if (!nearly_equal(area.value, 4.0f, 0.001f) || !nearly_equal(DEARUNITS_SQRT(area).value, 2.0f, 0.001f)) {
        return false;
    }

    hertz_t frequency = DEARUNITS_DIVIDE(1.0f, (second_t){ .value = 0.5f });
    if (!nearly_equal(frequency.value, 2.0f, 0.001f)) {
        return false;
    }

    // Trig accepts any angle unit; wrapping stays in the same unit.
    degree_t right_angle = { .value = 90.0f };
    if (!nearly_equal(DEARUNITS_SIN(right_angle), 1.0f, 0.001f)) {
        return false;
    }

    degree_t wrapped = DEARUNITS_WRAP_ANGLE((degree_t){ .value = 190.0f });
    if (!nearly_equal(wrapped.value, -170.0f, 0.01f)) {
        return false;
    }

    // Validity helpers
    meter_t bad_reading = { .value = __builtin_nanf("") };
    if (!DEARUNITS_IS_NAN(bad_reading) || DEARUNITS_IS_FINITE(bad_reading) || !DEARUNITS_IS_FINITE(arm)) {
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
