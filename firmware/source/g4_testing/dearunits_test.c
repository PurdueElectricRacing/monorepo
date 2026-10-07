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
volatile float dearunits_max_torque_nm    = 0.0f;
volatile float dearunits_pack_soc         = 0.0f;
volatile float dearunits_pack_kwh_used    = 0.0f;
volatile float dearunits_clamp_force_n    = 0.0f;

static bool nearly_equal(float a, float b, float tolerance) {
    float diff = (a > b) ? (a - b) : (b - a);
    return diff <= tolerance;
}

static bool test_drivetrain(void) {
    meter_t pedal = { .value = 0.03f };
    meter_t pedal_min = { .value = 0.0f };
    meter_t pedal_max = { .value = 0.05f };
    newton_meter_t torque_zero = { .value = 0.0f };
    newton_meter_t torque_full = { .value = 200.0f };
    newton_meter_t requested = DU_MAP_RANGE(pedal, pedal_min, pedal_max, torque_zero, torque_full);
    if (!nearly_equal(requested.value, 120.0f, 0.01f)) {
        return false;
    }

    newton_meter_t motor_limit = { .value = 100.0f };
    newton_meter_t limited = DU_SATURATE(requested, motor_limit);
    if (!nearly_equal(limited.value, 100.0f, 0.001f)) {
        return false;
    }

    newton_meter_t applied = { .value = 0.0f };
    newton_meter_t max_step = { .value = 30.0f };
    for (int tick = 0; tick < 4; tick++) {
        applied = DU_SLEW(applied, limited, max_step);
    }
    if (!nearly_equal(applied.value, 100.0f, 0.001f)) {
        return false;
    }

    newton_meter_t noise = { .value = 0.4f };
    newton_meter_t noise_floor = { .value = 0.5f };
    newton_meter_t filtered = DU_DEADBAND(noise, noise_floor);
    if (!nearly_equal(filtered.value, 0.0f, 0.001f)) {
        return false;
    }

    revolutions_per_minute_t motor_rpm = { .value = 6000.0f };
    radians_per_second_t motor_speed = DU_RADIANS_PER_SECOND_FROM(motor_rpm);
    watt_t shaft_power = DU_MULTIPLY(applied, motor_speed);
    if (!nearly_equal(shaft_power.value, 62831.85f, 1.0f)) {
        return false;
    }

    kilowatt_t power_cap_kw = { .value = 50.0f };
    watt_t power_cap = DU_WATT_FROM(power_cap_kw);
    watt_t allowed_power = DU_MIN(shaft_power, power_cap);
    newton_meter_t max_torque = DU_DIVIDE(allowed_power, motor_speed);
    dearunits_max_torque_nm = max_torque.value;
    if (!nearly_equal(max_torque.value, 79.5775f, 0.01f)) {
        return false;
    }

    newton_meter_t final_request = DU_MIN(applied, max_torque);
    if (!nearly_equal(final_request.value, 79.5775f, 0.01f)) {
        return false;
    }

    horsepower_t power_cap_hp = horsepower_from_watt(power_cap);
    if (!nearly_equal(power_cap_hp.value, 67.0511f, 0.001f)) {
        return false;
    }

    second_t control_period = { .value = 0.5f };
    joule_t segment_energy = DU_MULTIPLY(allowed_power, control_period);
    joule_t total_energy = DU_ADD(segment_energy, segment_energy);
    kilowatt_hour_t total_kwh = kilowatt_hour_from_joule(total_energy);
    return nearly_equal(total_kwh.value, 0.0138889f, 0.00001f);
}

static bool test_battery(void) {
    volt_t open_circuit = { .value = 400.0f };
    amp_t load_current = { .value = 60.0f };
    milliohm_t resistance_mohm = { .value = 50.0f };
    ohm_t resistance = DU_OHM_FROM(resistance_mohm);

    volt_t sag = DU_MULTIPLY(load_current, resistance);
    volt_t terminal = DU_SUBTRACT(open_circuit, sag);
    if (!nearly_equal(terminal.value, 397.0f, 0.001f)) {
        return false;
    }

    watt_t output_power = DU_MULTIPLY(terminal, load_current);
    watt_t loss = DU_MULTIPLY(sag, load_current);
    float efficiency = DU_DIVIDE(output_power, DU_ADD(output_power, loss));
    if (!nearly_equal(efficiency, 0.9925f, 0.0001f)) {
        return false;
    }

    second_t discharge_time = { .value = 1800.0f };
    coulomb_t drawn = DU_MULTIPLY(load_current, discharge_time);
    amp_hour_t capacity_ah = { .value = 100.0f };
    coulomb_t capacity = DU_COULOMB_FROM(capacity_ah);
    float state_of_charge = 1.0f - DU_DIVIDE(drawn, capacity);
    dearunits_pack_soc = state_of_charge;
    if (!nearly_equal(state_of_charge, 0.7f, 0.0001f)) {
        return false;
    }

    amp_hour_t drawn_ah = amp_hour_from_coulomb(drawn);
    if (!nearly_equal(drawn_ah.value, 30.0f, 0.001f)) {
        return false;
    }

    joule_t drawn_energy = DU_MULTIPLY(drawn, terminal);
    kilowatt_hour_t drawn_kwh = kilowatt_hour_from_joule(drawn_energy);
    dearunits_pack_kwh_used = drawn_kwh.value;
    if (!nearly_equal(drawn_kwh.value, 11.91f, 0.001f)) {
        return false;
    }

    volatile float zero_reading = 0.0f;
    amp_t no_current = { .value = zero_reading };
    volt_t no_voltage = { .value = zero_reading };
    ohm_t open_circuit_resistance = DU_DIVIDE(terminal, no_current);
    ohm_t undefined_resistance = DU_DIVIDE(no_voltage, no_current);
    if (DU_IS_FINITE(open_circuit_resistance) || DU_IS_NAN(open_circuit_resistance)) {
        return false;
    }
    return DU_IS_NAN(undefined_resistance) && DU_IS_FINITE(resistance);
}

static bool test_dynamics(void) {
    kilometers_per_hour_t cruise_kmh = { .value = 72.0f };
    meters_per_second_t cruise = DU_METERS_PER_SECOND_FROM(cruise_kmh);
    meters_per_second_t standstill = { .value = 0.0f };
    second_t launch_time = { .value = 4.0f };
    meters_per_second_squared_t longitudinal = DU_DIVIDE(DU_SUBTRACT(cruise, standstill), launch_time);
    if (!nearly_equal(longitudinal.value, 5.0f, 0.001f)) {
        return false;
    }

    meters_per_second_squared_t lateral = { .value = 12.0f };
    meters_per_second_squared_t combined = DU_HYPOT(longitudinal, lateral);
    standard_gravity_t combined_g = standard_gravity_from_meters_per_second_squared(combined);
    if (!nearly_equal(combined.value, 13.0f, 0.001f) || !nearly_equal(combined_g.value, 1.32568f, 0.0001f)) {
        return false;
    }

    kilogram_t car_mass = { .value = 300.0f };
    newton_t traction = DU_MULTIPLY(car_mass, longitudinal);
    watt_t traction_power = DU_MULTIPLY(traction, cruise);
    if (!nearly_equal(traction.value, 1500.0f, 0.001f) || !nearly_equal(traction_power.value, 30000.0f, 0.01f)) {
        return false;
    }

    meter_t launch_distance = DU_MULTIPLY(DU_MULTIPLY(0.5f, cruise), launch_time);
    if (!nearly_equal(launch_distance.value, 40.0f, 0.001f)) {
        return false;
    }

    second_t rotation_period = { .value = 0.05f };
    hertz_t rotation_rate = DU_DIVIDE(1.0f, rotation_period);
    second_t period_again = DU_DIVIDE(1.0f, rotation_rate);
    return nearly_equal(rotation_rate.value, 20.0f, 0.001f) && nearly_equal(period_again.value, 0.05f, 0.0001f);
}

static bool test_brakes(void) {
    bar_t line_pressure_bar = { .value = 40.0f };
    pascal_t line_pressure = DU_PASCAL_FROM(line_pressure_bar);
    centimeter_t bore_cm = { .value = 2.0f };
    meter_t bore = DU_METER_FROM(bore_cm);
    square_meter_t piston_area = DU_MULTIPLY(0.785398f, DU_SQUARE(bore));
    newton_t clamp_force = DU_MULTIPLY(line_pressure, piston_area);
    dearunits_clamp_force_n = clamp_force.value;
    if (!nearly_equal(clamp_force.value, 1256.64f, 0.05f)) {
        return false;
    }

    square_meter_t area_again = DU_DIVIDE(clamp_force, line_pressure);
    meter_t bore_again = DU_SQRT(DU_MULTIPLY(1.27324f, area_again));
    if (!nearly_equal(bore_again.value, 0.02f, 0.00001f)) {
        return false;
    }

    meter_t rotor_radius = { .value = 0.09f };
    newton_meter_t pad_torque = DU_MULTIPLY(DU_MULTIPLY(clamp_force, rotor_radius), 0.4f);
    return nearly_equal(pad_torque.value, 45.239f, 0.01f);
}

static bool test_signal_helpers(void) {
    fahrenheit_t coolant_f = { .value = 212.0f };
    celsius_t coolant = DU_CELSIUS_FROM(coolant_f);
    celsius_t coolant_limit = { .value = 85.0f };
    celsius_t reported = DU_MIN(coolant, coolant_limit);
    kelvin_t reported_k = kelvin_from_celsius(reported);
    if (!nearly_equal(reported.value, 85.0f, 0.01f) || !nearly_equal(reported_k.value, 358.15f, 0.01f)) {
        return false;
    }

    celsius_t derate_start = { .value = 60.0f };
    celsius_t derate_end = { .value = 80.0f };
    celsius_t motor_temp = { .value = 70.0f };
    newton_meter_t full = { .value = 100.0f };
    newton_meter_t none = { .value = 0.0f };
    newton_meter_t derated = DU_MAP_RANGE(motor_temp, derate_start, derate_end, full, none);
    float derate_fraction = DU_INVLERP(derate_start, derate_end, motor_temp);
    newton_meter_t interpolated = DU_LERP(full, none, derate_fraction);
    if (!nearly_equal(derated.value, 50.0f, 0.001f) || !nearly_equal(interpolated.value, 50.0f, 0.001f)) {
        return false;
    }

    celsius_t overtemp = { .value = 95.0f };
    newton_meter_t overtemp_torque = DU_CLAMP(DU_MAP_RANGE(overtemp, derate_start, derate_end, full, none), none, full);
    if (!nearly_equal(overtemp_torque.value, 0.0f, 0.001f)) {
        return false;
    }

    meter_t lap_length = { .value = 1000.0f };
    meter_t odometer = { .value = 3450.0f };
    meter_t into_lap = DU_FMOD(odometer, lap_length);
    float laps_driven = DU_DIVIDE(odometer, lap_length);
    if (!nearly_equal(into_lap.value, 450.0f, 0.001f) || !nearly_equal(laps_driven, 3.45f, 0.0001f)) {
        return false;
    }

    second_t best_lap = { .value = 61.7f };
    second_t previous_lap = { .value = 63.2f };
    second_t whole_seconds = DU_FLOOR(best_lap);
    second_t next_second = DU_CEIL(best_lap);
    second_t nearest_second = DU_ROUND(best_lap);
    second_t lap_delta = DU_ABS_DIFF(best_lap, previous_lap);
    if (!nearly_equal(whole_seconds.value, 61.0f, 0.001f) || !nearly_equal(next_second.value, 62.0f, 0.001f) ||
        !nearly_equal(nearest_second.value, 62.0f, 0.001f) || !nearly_equal(lap_delta.value, 1.5f, 0.001f)) {
        return false;
    }

    newton_meter_t drive_torque = { .value = 80.0f };
    newton_meter_t regen_torque = DU_NEGATE(drive_torque);
    newton_meter_t regen_limit = { .value = 25.0f };
    newton_meter_t regen_limited = DU_COPYSIGN(regen_limit, regen_torque);
    newton_meter_t regen_magnitude = DU_ABS(regen_limited);
    newton_meter_t regen_floor = DU_MAX(regen_torque, regen_limited);
    newton_meter_t tolerance = { .value = 0.001f };
    if (!nearly_equal(DU_SIGN(regen_torque), -1.0f, 0.001f) || !nearly_equal(DU_SIGN(drive_torque), 1.0f, 0.001f)) {
        return false;
    }
    if (!nearly_equal(regen_limited.value, -25.0f, 0.001f) || !nearly_equal(regen_floor.value, -25.0f, 0.001f)) {
        return false;
    }
    if (!DU_LT(regen_torque, drive_torque) || !DU_GT(drive_torque, regen_torque) ||
        !DU_LE(regen_magnitude, regen_limit) || !DU_GE(regen_limit, regen_magnitude)) {
        return false;
    }
    return DU_NEARLY_EQUAL(regen_magnitude, regen_limit, tolerance);
}

static bool run_dearunits_test(void) {
    mile_t one_mile = { .value = 1.0f };
    meter_t mile_in_meters = DU_METER_FROM(one_mile);
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

    meter_t travelled = DU_MULTIPLY(velocity, time);
    if (!nearly_equal(travelled.value, 100.0f, 0.001f)) {
        return false;
    }

    second_t travel_time = DU_DIVIDE(distance, velocity);
    if (!nearly_equal(travel_time.value, 10.0f, 0.001f)) {
        return false;
    }

    float distance_ratio = DU_DIVIDE(distance, travelled);
    if (!nearly_equal(distance_ratio, 1.0f, 0.001f)) {
        return false;
    }

    meter_t half_distance = DU_DIVIDE(distance, 2.0f);
    if (!nearly_equal(half_distance.value, 50.0f, 0.001f)) {
        return false;
    }

    meter_t clamped = DU_CLAMP(distance, arm, half_distance);
    if (!nearly_equal(clamped.value, 50.0f, 0.001f)) {
        return false;
    }

    if (!DU_LT(arm, distance) || DU_GT(arm, distance)) {
        return false;
    }

    meter_t gap = DU_ABS_DIFF(arm, distance);
    if (!nearly_equal(gap.value, 98.0f, 0.001f)) {
        return false;
    }

    if (!test_drivetrain() || !test_battery() || !test_dynamics() || !test_brakes() || !test_signal_helpers()) {
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
