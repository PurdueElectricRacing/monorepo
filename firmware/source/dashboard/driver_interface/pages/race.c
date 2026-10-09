/**
 * @file race.c
 * @brief Race page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "race.h"

#include "common/nextion/nextion.h"
#include "can_library/generated/DASHBOARD.h"
#include "pedals.h"
#include "common/utils/max.h"
#include "common/dearunits/generated/dearunits.h"
#include "colors.h"
#include "lap_timer.h"

static inline void update_car_state_telemetry() {
    if (can_data.main_hb.is_stale()) {
        NXT_setText(CAR_STATE, "STALE");
        NXT_setFontColor(CAR_STATE, WHITE);
        NXT_setBorderColor(CAR_STATE, WHITE);
        NXT_setBackground(CAR_STATE, BLACK);
        return;
    }

    switch (can_data.main_hb.car_state) {
        case CAR_STATE_FATAL:
            NXT_setText(CAR_STATE, "FATAL");
            NXT_setFontColor(CAR_STATE, RED);
            NXT_setBorderColor(CAR_STATE, RED);
            NXT_setBackground(CAR_STATE, MUTED_RED);
            break;
        case CAR_STATE_IDLE:
            NXT_setText(CAR_STATE, "IDLE");
            NXT_setFontColor(CAR_STATE, WHITE);
            NXT_setBorderColor(CAR_STATE, WHITE);
            NXT_setBackground(CAR_STATE, BLACK);
            break;
        case CAR_STATE_PRECHARGING:
            NXT_setText(CAR_STATE, "PRECHARGING");
            NXT_setFontColor(CAR_STATE, YELLOW);
            NXT_setBorderColor(CAR_STATE, YELLOW);
            NXT_setBackground(CAR_STATE, MUTED_YELLOW);
            break;
        case CAR_STATE_ENERGIZED:
            NXT_setText(CAR_STATE, "ENERGIZED");
            NXT_setFontColor(CAR_STATE, GREEN);
            NXT_setBorderColor(CAR_STATE, GREEN);
            NXT_setBackground(CAR_STATE, MUTED_GREEN);
            break;
        case CAR_STATE_BUZZING:
            NXT_setText(CAR_STATE, "BUZZING");
            NXT_setFontColor(CAR_STATE, YELLOW);
            NXT_setBorderColor(CAR_STATE, YELLOW);
            NXT_setBackground(CAR_STATE, MUTED_YELLOW);
            break;
        case CAR_STATE_READY2DRIVE:
            NXT_setText(CAR_STATE, "READY2DRIVE");
            NXT_setFontColor(CAR_STATE, GREEN);
            NXT_setBorderColor(CAR_STATE, GREEN);
            NXT_setBackground(CAR_STATE, MUTED_GREEN);
            break;
    }
}

static inline void update_motor_telemetry() {
    if (can_data.motor_temps.is_stale()) {
        NXT_setText(MOTOR_TEMP, "S");
    } else {
        int16_t max_motor_temp = MAXOF(
            can_data.motor_temps.front_right,
            can_data.motor_temps.front_left,
            can_data.motor_temps.rear_left,
            can_data.motor_temps.rear_right
        );

        int16_t scaled_motor_temp = (int16_t)(max_motor_temp * UNPACK_COEFF_MOTOR_TEMPS_FRONT_RIGHT);
        NXT_setTextFormatted(MOTOR_TEMP, "%dC", scaled_motor_temp);
    }
}

static inline void update_igbt_telemetry() {
    if (can_data.igbt_temps.is_stale()) {
        NXT_setText(IGBT_TEMP, "S");
    } else {
        int16_t max_igbt_temp = MAXOF(
            can_data.igbt_temps.front_right,
            can_data.igbt_temps.front_left,
            can_data.igbt_temps.rear_left,
            can_data.igbt_temps.rear_right
        );

        int16_t scaled_igbt_temp = (int16_t)(max_igbt_temp * UNPACK_COEFF_IGBT_TEMPS_FRONT_RIGHT);
        NXT_setTextFormatted(IGBT_TEMP, "%dC", scaled_igbt_temp);
    }
}

static inline void update_pack_telemetry() {
    if (can_data.pack_bms.is_stale()) {
        NXT_setText(BATT_VOLT, "S");
        NXT_setText(BATT_TEMP, "S");
    } else {
        uint16_t scaled_voltage = (uint16_t)(can_data.pack_bms.pack_voltage * UNPACK_COEFF_PACK_BMS_PACK_VOLTAGE);
        NXT_setTextFormatted(BATT_VOLT, "%dV", scaled_voltage);
        NXT_setTextFormatted(BATT_TEMP, "%dC", can_data.pack_bms.max_temp);
    }

    if (can_data.pack_analog.is_stale()) {
        NXT_setText(BATT_CURR, "S");
    } else {
        int16_t scaled_current = (int16_t)(can_data.pack_analog.pack_current * UNPACK_COEFF_PACK_ANALOG_PACK_CURRENT);
        NXT_setTextFormatted(BATT_CURR, "%dA", scaled_current);
    }
}

static inline void update_speed_telemetry() {
    if (can_data.motor_speeds.is_stale()) {
        NXT_setText(SPEED, "S");
        return;
    }
    
    if (can_data.motor_speeds.rear_left < 0) {
        NXT_setText(SPEED, "NEG");
        return;
    }

    int16_t max_motor_rpm = MAXOF(
        can_data.motor_speeds.front_right,
        can_data.motor_speeds.front_left,
        can_data.motor_speeds.rear_left,
        can_data.motor_speeds.rear_right
    );

    // vehicle constants
    static constexpr float WHEEL_RADIUS_IN = 8.0f;
    static constexpr float GEAR_RATIO      = 12.51f;

    meter_t wheel_radius = DU_METER_FROM((inch_t) {WHEEL_RADIUS_IN});

    // radial speed to linear speed (assuming no slip)
    revolutions_per_minute_t motor_rpm = {.value = (float)max_motor_rpm};
    revolutions_per_minute_t wheel_rpm = DU_DIVIDE(motor_rpm, GEAR_RATIO);
    radians_per_second_t wheel_rps     = DU_RADIANS_PER_SECOND_FROM(wheel_rpm);
    meters_per_second_t vehicle_speed  = DU_MULTIPLY(wheel_rps, wheel_radius);

    // convert to mph for display
    miles_per_hour_t vehicle_mph = miles_per_hour_from_meters_per_second(vehicle_speed);
    NXT_setTextFormatted(SPEED, "%d", (int16_t)vehicle_mph.value);
}

static inline void update_pedal_telemetry() {
    // The nextion object expects [0-100]
    NXT_setValue(THROT_BAR, pedal_values.throttle);
    NXT_setValue(RGN_BAR, pedal_values.regen);
    NXT_setValue(BRK_BAR, pedal_values.brake);
}

// this is diddy but whatever
static inline void update_tv_bar(char* obj_name, int16_t torque_req) {
    // The nextion object expects [0-100]
    if (torque_req < 0) { // regen
        uint16_t regen_req = (uint16_t)(torque_req * -1);
        NXT_setFontColor(obj_name, GREEN);
        NXT_setValue(obj_name, regen_req);
    } else { // vector request
        uint16_t scaled_req = (uint16_t)(torque_req / 2.1f);
        NXT_setFontColor(obj_name, BLUE);
        NXT_setValue(obj_name, scaled_req);
    }
}

static inline void update_tv_telemetry() {
    update_tv_bar(FR_BAR, can_data.vcu_torque_request.front_right);
    update_tv_bar(FL_BAR, can_data.vcu_torque_request.front_left);
    update_tv_bar(RL_BAR, can_data.vcu_torque_request.rear_left);
    update_tv_bar(RR_BAR, can_data.vcu_torque_request.rear_right);
}

static inline void update_lap_time_telemetry() {
    uint32_t elapsed_ms = lap_timer_elapsed_ms();
    uint32_t minutes = elapsed_ms / 60000U;
    uint32_t seconds = (elapsed_ms / 1000U) % 60U;
    uint32_t centiseconds = (elapsed_ms / 10U) % 100U;

    NXT_setTextFormatted(
        LAP_TIME,
        "%02lu:%02lu.%02lu",
        (unsigned long)minutes,
        (unsigned long)seconds,
        (unsigned long)centiseconds
    );
}

/**
 * @brief Updates telemetry data on the race dashboard LCD display
 *
 * Only updates on race page. Displays 'S' for stale values.
 */
void race_telemetry_update() {
    update_pedal_telemetry();
    update_car_state_telemetry();
    update_speed_telemetry();
    update_motor_telemetry();
    update_igbt_telemetry();
    update_pack_telemetry();
    update_tv_telemetry();
    update_lap_time_telemetry();
}
