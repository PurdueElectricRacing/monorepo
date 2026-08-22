/**
 * @file control_loop.c
 * @brief Main torque vectoring control loop
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Trevor Koessler (tkoessle@purdue.edu)
 */

#include <stdint.h>

#include "can_library/generated/TORQUE_VECTOR.h"
#include "common/utils/max.h"
#include "sensors.h"
#include "vcu.h"

static pVCU_struct pVCU;
static xVCU_struct xVCU;
static yVCU_struct yVCU;

// init default settings
volatile vcu_mode_t vcu_mode = VCU_MODE_TUNING;
volatile bool is_tv_enabled = true;
volatile bool is_regen_enabled = true;
volatile vcu_settings_data_t vcu_settings[5] = {
    [VCU_MODE_ACCEL] = {
        .lateral_gain = 50,
        .longitudinal_gain = 50,
        .electronic_brake_bias = 50
    },
    [VCU_MODE_SKIDPAD] = {
        .lateral_gain = 50,
        .longitudinal_gain = 50,
        .electronic_brake_bias = 50
    },
    [VCU_MODE_AUTOCROSS] = {
        .lateral_gain = 50,
        .longitudinal_gain = 50,
        .electronic_brake_bias = 50
    },
    [VCU_MODE_ENDURANCE] = {
        .lateral_gain = 50,
        .longitudinal_gain = 50,
        .electronic_brake_bias = 50
    },
    [VCU_MODE_TUNING] = {
        .lateral_gain = 50,
        .longitudinal_gain = 50,
        .electronic_brake_bias = 50
    }
};
volatile uint32_t last_vcu_settings_tx = 0;

static_assert(VCU_SETTINGS_LAYOUT_HASH == VCU_DRIVER_REQUEST_LAYOUT_HASH);

void control_init(void) {
    // TV initialization
    pVCU = init_pVCU();
    xVCU = init_xVCU();
    yVCU = init_yVCU();
}

static inline void report_vcu_settings() {
    const volatile vcu_settings_data_t *current_settings = &vcu_settings[vcu_mode];

    CAN_SEND_vcu_settings(
        vcu_mode,
        current_settings->lateral_gain,
        current_settings->longitudinal_gain,
        current_settings->electronic_brake_bias,
        is_regen_enabled,
        is_tv_enabled
    );
}

void vcu_driver_request_CALLBACK(void) {
    vcu_mode_t requested_mode = can_data.vcu_driver_request.vcu_mode;
    if (vcu_mode != requested_mode) {
        vcu_mode = requested_mode;
        return;
    }

    vcu_settings[vcu_mode].lateral_gain = can_data.vcu_driver_request.lateral_gain;
    vcu_settings[vcu_mode].longitudinal_gain = can_data.vcu_driver_request.longitudinal_gain;
    vcu_settings[vcu_mode].electronic_brake_bias = can_data.vcu_driver_request.electronic_brake_bias;
    is_regen_enabled = can_data.vcu_driver_request.is_regen_enabled;
    is_tv_enabled = can_data.vcu_driver_request.is_tv_enabled;

    // echo back the current settings
    report_vcu_settings();
}

static inline bool is_any_crit_stale() {
    return can_data.pedals.is_stale()
        || can_data.steering_angle.is_stale()
        || can_data.wheel_speeds.is_stale()
        || can_data.IZZE_angular_rate.is_stale();
}

void control_loop() {
    volatile uint32_t now = xTaskGetTickCount();
    if ((now - last_vcu_settings_tx) >= VCU_SETTINGS_PERIOD_MS) { // periodic sync
        report_vcu_settings();
        last_vcu_settings_tx = now;
    }

    if (is_any_crit_stale()) {
        is_tv_enabled = false;
        return;
    }

    // load up the xVCU (input) struct with most recent data
    xVCU.VCU_MODE_REQ = vcu_mode;
    xVCU.REGEN_EN = is_regen_enabled;

    xVCU.THROT_RAW = can_data.pedals.throttle / 100.0f;
    xVCU.REGEN_RAW = can_data.pedals.regen / 100.0f;
    xVCU.BRAKE_RAW = can_data.pedals.brake / 100.0f;

    xVCU.ST_RAW = can_data.steering_angle.angle * UNPACK_COEFF_STEERING_ANGLE_ANGLE * -1;
    xVCU.VB_RAW = can_data.pack_stats.pack_voltage * UNPACK_COEFF_PACK_STATS_PACK_VOLTAGE;
    static constexpr float RPM_TO_RADS = 2.0f * 3.14f / 60.0f;
    xVCU.WM_RAW[0] = can_data.wheel_speeds.front_left * RPM_TO_RADS;
    xVCU.WM_RAW[1] = can_data.wheel_speeds.front_right * RPM_TO_RADS;
    xVCU.WM_RAW[2] = can_data.wheel_speeds.rear_left * RPM_TO_RADS;
    xVCU.WM_RAW[3] = can_data.wheel_speeds.rear_right * RPM_TO_RADS;
    xVCU.GS_RAW = nav_pvt.groundSpeed * 1E-3f; // convert mm/s to m/s

    static constexpr float DEG_TO_RAD = 3.14f / 180.0f;
    xVCU.AV_RAW[0] = imu_data.gyro_x * DEG_TO_RAD;
    xVCU.AV_RAW[1] = imu_data.gyro_y * DEG_TO_RAD;
    xVCU.AV_RAW[2] = imu_data.gyro_z * DEG_TO_RAD;
    xVCU.IB_RAW = can_data.pack_stats.pack_current * UNPACK_COEFF_PACK_STATS_PACK_CURRENT;

    int16_t max_motor_temp = MAXOF(
        can_data.motor_temps.front_right,
        can_data.motor_temps.front_left,
        can_data.motor_temps.rear_left,
        can_data.motor_temps.rear_right
    );
    xVCU.MT_RAW = max_motor_temp * UNPACK_COEFF_MOTOR_TEMPS_FRONT_RIGHT;

    int16_t max_igbt_temp = MAXOF(
        can_data.igbt_temps.front_right,
        can_data.igbt_temps.front_left,
        can_data.igbt_temps.rear_left,
        can_data.igbt_temps.rear_right
    );
    xVCU.IGBT_T_RAW = max_igbt_temp * UNPACK_COEFF_IGBT_TEMPS_FRONT_RIGHT;

    xVCU.BT_RAW = 30.0f; // ! hardcoded to 30C for now

    // load up driver settings
    const volatile vcu_settings_data_t *current_settings = &vcu_settings[vcu_mode];
    xVCU.RG_FR_split_RAW = current_settings->electronic_brake_bias;
    xVCU.SK_LR_gain_RAW  = vcu_settings[VCU_MODE_SKIDPAD].lateral_gain;
    xVCU.SK_FR_split_RAW = vcu_settings[VCU_MODE_SKIDPAD].longitudinal_gain;
    xVCU.AX_LR_control_force_RAW  = vcu_settings[VCU_MODE_AUTOCROSS].lateral_gain;
    xVCU.AX_FR_split_RAW = vcu_settings[VCU_MODE_AUTOCROSS].longitudinal_gain;
    xVCU.TS_LR_split_RAW = vcu_settings[VCU_MODE_TUNING].lateral_gain;
    xVCU.TS_FR_split_RAW = vcu_settings[VCU_MODE_TUNING].longitudinal_gain;

    // step the VCU model
    vcu_step(&pVCU, &xVCU, &yVCU);

    static constexpr float NM_TO_PCT = (1.0f / 9.8f) * 100.0f;
    float torque_front_right_pct = yVCU.TORQUE_OUT[1] * NM_TO_PCT;
    float torque_front_left_pct  = yVCU.TORQUE_OUT[0] * NM_TO_PCT;
    float torque_rear_left_pct   = yVCU.TORQUE_OUT[2] * NM_TO_PCT;
    float torque_rear_right_pct  = yVCU.TORQUE_OUT[3] * NM_TO_PCT;

    // send outputs on CAN
    CAN_SEND_vcu_torque_request(
        torque_front_right_pct,
        torque_front_left_pct,
        torque_rear_left_pct,
        torque_rear_right_pct
    );
}
