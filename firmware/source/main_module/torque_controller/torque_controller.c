/**
 * @file torque_controller.c
 * @brief Vehicle torque request calculation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "torque_controller.h"

#include "can_library/generated/MAIN_MODULE.h"
#include "common/utils/min.h"

// For speed calculations
static constexpr float WHEEL_RADIUS_IN = 8.0f;
static constexpr float GEAR_RATIO = 12.51f;
static constexpr float WHEEL_CIRCUMFERENCE_IN = 2.0f * 3.14159f * WHEEL_RADIUS_IN;
static constexpr float OUTPUT_REV_PER_MOTOR_REV = 1.0f / GEAR_RATIO;
static constexpr float INCHES_PER_MOTOR_REV = WHEEL_CIRCUMFERENCE_IN * OUTPUT_REV_PER_MOTOR_REV;
static constexpr float MINUTES_PER_HOUR = 60.0f;
static constexpr float INCHES_PER_MILE = 63360.0f;
static constexpr float RPM_TO_MPH = INCHES_PER_MOTOR_REV * MINUTES_PER_HOUR / INCHES_PER_MILE;

static torque_request_t zero_torque_request(void) {
    return (torque_request_t) {
        .front_left  = 0,
        .front_right = 0,
        .rear_left   = 0,
        .rear_right  = 0,
    };
}

static torque_request_t direct_mapped_regen(void) {
    // Map brake [0, 100] to torque [0, -100]
    int16_t regen_torque = (int16_t)(can_data.pedals.brake * -1.0f);

    return (torque_request_t) {
        .front_left  = regen_torque,
        .front_right = regen_torque,
        .rear_left   = regen_torque,
        .rear_right  = regen_torque,
    };
}

static torque_request_t direct_mapped_throttle(void) {
    // Map throttle [0, 100] to torque [0, 210]
    int16_t rear_torque = (int16_t)(can_data.pedals.throttle * 2.1f);

    // Bias to feel like a 40% - 60% torque split
    int16_t front_torque = (int16_t)((40.0f / 60.0f) * rear_torque);

    return (torque_request_t) {
        .front_left  = front_torque,
        .front_right = front_torque,
        .rear_left   = rear_torque,
        .rear_right  = rear_torque,
    };
}

static torque_request_t torque_vectoring_request(void) {
    // use torque request from VCU
    return (torque_request_t) {
        .front_left  = can_data.vcu_torque_request.front_left,
        .front_right = can_data.vcu_torque_request.front_right,
        .rear_left   = can_data.vcu_torque_request.rear_left,
        .rear_right  = can_data.vcu_torque_request.rear_right,
    };
}

torque_request_t torque_controller_get_request(void) {
    if (can_data.pedals.is_stale()) {
        return zero_torque_request();
    }

    bool is_tv_stale = can_data.vcu_settings.is_stale() || can_data.vcu_torque_request.is_stale();
    if (!is_tv_stale && can_data.vcu_settings.is_tv_enabled) {
        return torque_vectoring_request();
    }

    bool is_braking = can_data.pedals.brake > 5;
    int16_t min_wheelspeed = MINOF(
        g_powertrain.front_right.crit->AMK_ActualSpeed,
        g_powertrain.front_left.crit->AMK_ActualSpeed,
        g_powertrain.rear_left.crit->AMK_ActualSpeed,
        g_powertrain.rear_right.crit->AMK_ActualSpeed
    );
    bool is_vehicle_speed_high = min_wheelspeed * RPM_TO_MPH > 5;
    bool is_pack_low_enough = can_data.pack_stats.pack_voltage < 470;
    bool is_regen_allowed = is_braking && is_vehicle_speed_high && is_pack_low_enough;

    if (can_data.pedals.throttle > 0) {
        return direct_mapped_throttle();
    }
    if (is_regen_allowed) {
        return direct_mapped_regen();
    }
    return zero_torque_request();
}
