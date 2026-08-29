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
#include "colors.h"

// For speed calcs
static constexpr float WHEEL_RADIUS_IN = 8.0f;
static constexpr float GEAR_RATIO = 12.51f;

static constexpr float WHEEL_CIRCUMFERENCE_IN = 2.0f * 3.14159f * WHEEL_RADIUS_IN;
static constexpr float OUTPUT_REV_PER_MOTOR_REV = 1.0f / GEAR_RATIO;
static constexpr float INCHES_PER_MOTOR_REV = WHEEL_CIRCUMFERENCE_IN * OUTPUT_REV_PER_MOTOR_REV;

static constexpr float MINUTES_PER_HOUR = 60.0f;
static constexpr float INCHES_PER_MILE = 63360.0f;

static constexpr float RPM_TO_MPH = INCHES_PER_MOTOR_REV * MINUTES_PER_HOUR / INCHES_PER_MILE;

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

        int16_t scaled_motor_temp =
            (int16_t)(max_motor_temp * UNPACK_COEFF_MOTOR_TEMPS_FRONT_RIGHT);
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
    if (can_data.pack_stats.is_stale()) {
        NXT_setText(BATT_VOLT, "S");
        NXT_setText(BATT_CURR, "S");
        NXT_setText(BATT_TEMP, "S");
    } else {
        uint16_t scaled_voltage =
            (uint16_t)(can_data.pack_stats.pack_voltage * UNPACK_COEFF_PACK_STATS_PACK_VOLTAGE);
        int16_t scaled_current =
            (int16_t)(can_data.pack_stats.pack_current * UNPACK_COEFF_PACK_STATS_PACK_CURRENT);
        NXT_setTextFormatted(BATT_VOLT, "%dV", scaled_voltage);
        NXT_setTextFormatted(BATT_CURR, "%dA", scaled_current);
        NXT_setTextFormatted(BATT_TEMP, "%dC", can_data.pack_stats.max_temp);
    }
}

// todo better speed calc lol
static inline void update_speed_telemetry() {
    if (can_data.wheel_speeds.is_stale()) {
        NXT_setText(SPEED, "S");
    } else {
        if (can_data.wheel_speeds.rear_left < 0) {
            NXT_setText(SPEED, "NEG");
        } else {
            int16_t max_wheelspeed = MAXOF(
                can_data.wheel_speeds.front_right,
                can_data.wheel_speeds.front_left,
                can_data.wheel_speeds.rear_left,
                can_data.wheel_speeds.rear_right
            );

            int16_t mph = (int16_t)(max_wheelspeed * RPM_TO_MPH);
            NXT_setTextFormatted(SPEED, "%d", mph);
        }
    }
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
}