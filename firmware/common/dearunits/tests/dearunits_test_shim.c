#include "dearunits_test_shim.h"
#include "common/dearunits/generated/dear_units.h"

// Keep C23 dispatch in C; Google Test owns assertions and diagnostics.
#define CHECK_NEAR(actual, expected, tolerance) \
    check((actual), (expected), (tolerance), #actual, __LINE__)
#define CHECK_TRUE(condition) CHECK_NEAR((condition) ? 1.0f : 0.0f, 1.0f, 0.0f)

void dearunits_test_drivetrain(dearunits_check_fn check) {
    newton_meter_t requested = { .value = 120.0f };

    newton_meter_t motor_limit = { .value = 100.0f };
    newton_meter_t applied = DU_MIN(requested, motor_limit);
    CHECK_NEAR(applied.value, 100.0f, 0.001f);

    revolutions_per_minute_t motor_rpm = { .value = 6000.0f };
    radians_per_second_t motor_speed = DU_RADIANS_PER_SECOND_FROM(motor_rpm);
    watt_t shaft_power = DU_MULTIPLY(applied, motor_speed);
    CHECK_NEAR(shaft_power.value, 62831.85f, 1.0f);

    kilowatt_t power_cap_kw = { .value = 50.0f };
    watt_t power_cap = DU_WATT_FROM(power_cap_kw);
    watt_t allowed_power = DU_MIN(shaft_power, power_cap);
    newton_meter_t max_torque = DU_DIVIDE(allowed_power, motor_speed);
    CHECK_NEAR(max_torque.value, 79.5775f, 0.01f);

    newton_meter_t final_request = DU_MIN(applied, max_torque);
    CHECK_NEAR(final_request.value, 79.5775f, 0.01f);

    second_t control_period = { .value = 0.5f };
    joule_t segment_energy = DU_MULTIPLY(allowed_power, control_period);
    joule_t total_energy = DU_ADD(segment_energy, segment_energy);
    kilowatt_hour_t total_kwh = kilowatt_hour_from_joule(total_energy);
    CHECK_NEAR(total_kwh.value, 0.0138889f, 0.00001f);
}

void dearunits_test_battery(dearunits_check_fn check) {
    volt_t open_circuit = { .value = 400.0f };
    amp_t load_current = { .value = 60.0f };
    milliohm_t resistance_mohm = { .value = 50.0f };
    ohm_t resistance = DU_OHM_FROM(resistance_mohm);

    volt_t sag = DU_MULTIPLY(load_current, resistance);
    volt_t terminal = DU_SUBTRACT(open_circuit, sag);
    CHECK_NEAR(terminal.value, 397.0f, 0.001f);

    watt_t output_power = DU_MULTIPLY(terminal, load_current);
    watt_t loss = DU_MULTIPLY(sag, load_current);
    float efficiency = DU_DIVIDE(output_power, DU_ADD(output_power, loss));
    CHECK_NEAR(efficiency, 0.9925f, 0.0001f);

    second_t discharge_time = { .value = 1800.0f };
    coulomb_t drawn = DU_MULTIPLY(load_current, discharge_time);
    amp_hour_t capacity_ah = { .value = 100.0f };
    coulomb_t capacity = DU_COULOMB_FROM(capacity_ah);
    float state_of_charge = 1.0f - DU_DIVIDE(drawn, capacity);
    CHECK_NEAR(state_of_charge, 0.7f, 0.0001f);

    amp_hour_t drawn_ah = amp_hour_from_coulomb(drawn);
    CHECK_NEAR(drawn_ah.value, 30.0f, 0.001f);

    joule_t drawn_energy = DU_MULTIPLY(drawn, terminal);
    kilowatt_hour_t drawn_kwh = kilowatt_hour_from_joule(drawn_energy);
    CHECK_NEAR(drawn_kwh.value, 11.91f, 0.001f);
}

void dearunits_test_dynamics(dearunits_check_fn check) {
    kilometers_per_hour_t cruise_kmh = { .value = 72.0f };
    meters_per_second_t cruise = DU_METERS_PER_SECOND_FROM(cruise_kmh);
    meters_per_second_t standstill = { .value = 0.0f };
    second_t launch_time = { .value = 4.0f };
    meters_per_second_squared_t longitudinal = DU_DIVIDE(DU_SUBTRACT(cruise, standstill), launch_time);
    CHECK_NEAR(longitudinal.value, 5.0f, 0.001f);

    kilogram_t car_mass = { .value = 300.0f };
    newton_t traction = DU_MULTIPLY(car_mass, longitudinal);
    watt_t traction_power = DU_MULTIPLY(traction, cruise);
    CHECK_NEAR(traction.value, 1500.0f, 0.001f);
    CHECK_NEAR(traction_power.value, 30000.0f, 0.01f);

    meter_t launch_distance = DU_MULTIPLY(DU_MULTIPLY(0.5f, cruise), launch_time);
    CHECK_NEAR(launch_distance.value, 40.0f, 0.001f);

    second_t rotation_period = { .value = 0.05f };
    hertz_t rotation_rate = DU_DIVIDE(1.0f, rotation_period);
    second_t period_again = DU_DIVIDE(1.0f, rotation_rate);
    CHECK_NEAR(rotation_rate.value, 20.0f, 0.001f);
    CHECK_NEAR(period_again.value, 0.05f, 0.0001f);
}

void dearunits_test_brakes(dearunits_check_fn check) {
    bar_t line_pressure_bar = { .value = 40.0f };
    pascal_t line_pressure = DU_PASCAL_FROM(line_pressure_bar);
    centimeter_t bore_cm = { .value = 2.0f };
    meter_t bore = DU_METER_FROM(bore_cm);
    square_meter_t piston_area = DU_MULTIPLY(0.785398f, DU_SQUARE(bore));
    newton_t clamp_force = DU_MULTIPLY(line_pressure, piston_area);
    CHECK_NEAR(clamp_force.value, 1256.64f, 0.05f);

    square_meter_t area_again = DU_DIVIDE(clamp_force, line_pressure);
    meter_t bore_again = DU_SQRT(DU_MULTIPLY(1.27324f, area_again));
    CHECK_NEAR(bore_again.value, 0.02f, 0.00001f);

    meter_t rotor_radius = { .value = 0.09f };
    newton_meter_t pad_torque = DU_MULTIPLY(DU_MULTIPLY(clamp_force, rotor_radius), 0.4f);
    CHECK_NEAR(pad_torque.value, 45.239f, 0.01f);
}

void dearunits_test_signal_helpers(dearunits_check_fn check) {
    fahrenheit_t coolant_f = { .value = 212.0f };
    celsius_t coolant = DU_CELSIUS_FROM(coolant_f);
    celsius_t coolant_limit = { .value = 85.0f };
    celsius_t reported = DU_MIN(coolant, coolant_limit);
    fahrenheit_t reported_f = fahrenheit_from_celsius(reported);
    CHECK_NEAR(reported.value, 85.0f, 0.01f);
    CHECK_NEAR(reported_f.value, 185.0f, 0.01f);

    second_t best_lap = { .value = 61.7f };
    second_t previous_lap = { .value = 63.2f };
    second_t whole_seconds = DU_FLOOR(best_lap);
    second_t next_second = DU_CEIL(best_lap);
    second_t nearest_second = DU_ROUND(best_lap);
    second_t lap_delta = DU_ABS_DIFF(best_lap, previous_lap);
    CHECK_NEAR(whole_seconds.value, 61.0f, 0.001f);
    CHECK_NEAR(next_second.value, 62.0f, 0.001f);
    CHECK_NEAR(nearest_second.value, 62.0f, 0.001f);
    CHECK_NEAR(lap_delta.value, 1.5f, 0.001f);

    newton_meter_t drive_torque = { .value = 80.0f };
    newton_meter_t regen_torque = DU_NEGATE(drive_torque);
    newton_meter_t regen_limit = { .value = 25.0f };
    newton_meter_t regen_limited = DU_NEGATE(regen_limit);
    newton_meter_t regen_magnitude = DU_ABS(regen_limited);
    newton_meter_t regen_floor = DU_MAX(regen_torque, regen_limited);
    newton_meter_t tolerance = { .value = 0.001f };
    CHECK_NEAR(regen_limited.value, -25.0f, 0.001f);
    CHECK_NEAR(regen_floor.value, -25.0f, 0.001f);

    CHECK_TRUE(DU_LT(regen_torque, drive_torque));
    CHECK_TRUE(DU_GT(drive_torque, regen_torque));
    CHECK_TRUE(DU_LE(regen_magnitude, regen_limit));
    CHECK_TRUE(DU_GE(regen_limit, regen_magnitude));

    CHECK_TRUE(DU_NEARLY_EQUAL(regen_magnitude, regen_limit, tolerance));
}

void dearunits_test_basics(dearunits_check_fn check) {
    mile_t one_mile = { .value = 1.0f };
    meter_t mile_in_meters = DU_METER_FROM(one_mile);
    CHECK_NEAR(mile_in_meters.value, 1609.344f, 0.001f);

    meter_t distance = { .value = 100.0f };
    second_t time = { .value = 10.0f };
    meters_per_second_t velocity = velocity_from(distance, time);
    CHECK_NEAR(velocity.value, 10.0f, 0.001f);

    miles_per_hour_t velocity_mph = miles_per_hour_from_meters_per_second(velocity);
    CHECK_NEAR(velocity_mph.value, 22.3694f, 0.01f);

    // A compound type built from base-class values (force = mass * length / time^2)...
    kilogram_t mass = { .value = 10.0f };
    meter_t arm = { .value = 2.0f };
    second_t interval = { .value = 1.0f };
    newton_t force = force_from(mass, arm, interval);
    CHECK_NEAR(force.value, 20.0f, 0.001f);

    newton_meter_t torque = torque_from(force, arm);
    CHECK_NEAR(torque.value, 40.0f, 0.001f);

    meter_t travelled = DU_MULTIPLY(velocity, time);
    CHECK_NEAR(travelled.value, 100.0f, 0.001f);

    second_t travel_time = DU_DIVIDE(distance, velocity);
    CHECK_NEAR(travel_time.value, 10.0f, 0.001f);

    float distance_ratio = DU_DIVIDE(distance, travelled);
    CHECK_NEAR(distance_ratio, 1.0f, 0.001f);

    meter_t half_distance = DU_DIVIDE(distance, 2.0f);
    CHECK_NEAR(half_distance.value, 50.0f, 0.001f);

    meter_t clamped = DU_CLAMP(distance, arm, half_distance);
    CHECK_NEAR(clamped.value, 50.0f, 0.001f);

    CHECK_TRUE(DU_LT(arm, distance));
    CHECK_TRUE(!DU_GT(arm, distance));

    meter_t gap = DU_ABS_DIFF(arm, distance);
    CHECK_NEAR(gap.value, 98.0f, 0.001f);
}
