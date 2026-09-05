/**
 * @file vehicle_fsm.c
 * @brief Master vehicle state machine implementation
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "vehicle_fsm.h"

#include "can_library/faults_common.h"
#include "can_library/generated/MAIN_MODULE.h"
#include "common/phal/gpio.h"
#include "main.h"
#include "common/utils/min.h"

// For speed calcs
static constexpr float WHEEL_RADIUS_IN = 8.0f;
static constexpr float GEAR_RATIO = 12.51f;
static constexpr float WHEEL_CIRCUMFERENCE_IN = 2.0f * 3.14159f * WHEEL_RADIUS_IN;
static constexpr float OUTPUT_REV_PER_MOTOR_REV = 1.0f / GEAR_RATIO;
static constexpr float INCHES_PER_MOTOR_REV = WHEEL_CIRCUMFERENCE_IN * OUTPUT_REV_PER_MOTOR_REV;
static constexpr float MINUTES_PER_HOUR = 60.0f;
static constexpr float INCHES_PER_MILE = 63360.0f;
static constexpr float RPM_TO_MPH = INCHES_PER_MOTOR_REV * MINUTES_PER_HOUR / INCHES_PER_MILE;

// Global data structures
car_t g_car;
torque_request_t g_torque_request;

static torque_request_t zero_torque_request() {
    torque_request_t torque_request = {
        .front_left  = 0,
        .front_right = 0,
        .rear_left   = 0,
        .rear_right  = 0
    };

    return torque_request;
}

static torque_request_t direct_mapped_regen() {
    // Map brake [0, 100] to torque [0, -100]
    int16_t regen_torque = can_data.pedals.brake * -1.0f;

    torque_request_t torque_request = {
        .front_left  = regen_torque,
        .front_right = regen_torque,
        .rear_left   = regen_torque,
        .rear_right  = regen_torque
    };

    return torque_request;
}

static torque_request_t direct_mapped_throttle() {
    // Map throttle [0, 100] to torque [0, 210]
    int16_t rear_torque = can_data.pedals.throttle * 2.1f;
    
    // Bias to feel like a 40% - 60% torque split
    int16_t front_torque = (40.0f / 60.0f) * rear_torque;

    torque_request_t torque_request = {
        .front_left  = front_torque,
        .front_right = front_torque,
        .rear_left   = rear_torque,
        .rear_right  = rear_torque
    };

    return torque_request;
}

static void update_torque_request() {
    if (can_data.pedals.is_stale()) {
        g_torque_request.front_right = 0;
        g_torque_request.front_left  = 0;
        g_torque_request.rear_left   = 0;
        g_torque_request.rear_right  = 0;
        return;
    }

    bool is_tv_stale = can_data.vcu_settings.is_stale() || can_data.vcu_torque_request.is_stale();
    if (!is_tv_stale && can_data.vcu_settings.is_tv_enabled) {
        // Forward TV Requested Torques
        g_torque_request.front_right = can_data.vcu_torque_request.front_right;
        g_torque_request.front_left  = can_data.vcu_torque_request.front_left;
        g_torque_request.rear_left   = can_data.vcu_torque_request.rear_left;
        g_torque_request.rear_right  = can_data.vcu_torque_request.rear_right;
        return;
    }

    // regen guards
    bool is_braking = (can_data.pedals.brake) > 5;
    int16_t min_wheelspeed = MINOF(
        g_car.front_right.crit->AMK_ActualSpeed,
        g_car.front_left.crit->AMK_ActualSpeed,
        g_car.rear_left.crit->AMK_ActualSpeed,
        g_car.rear_right.crit->AMK_ActualSpeed
    );

    float vehicle_speed_mph = (float)min_wheelspeed * RPM_TO_MPH;
    float pack_voltage = can_data.pack_stats.pack_voltage * UNPACK_COEFF_PACK_STATS_PACK_VOLTAGE;

    bool is_vehicle_speed_high = vehicle_speed_mph > 5.0f;
    bool is_pack_low_enough = pack_voltage < 470.0f;
    bool is_regen_allowed = is_braking && is_vehicle_speed_high && is_pack_low_enough;

    if (can_data.pedals.throttle > 0) {
        g_torque_request = direct_mapped_throttle();
    } else if (is_regen_allowed) {
        g_torque_request = direct_mapped_regen();
    } else {
        g_torque_request = zero_torque_request();
    }
}

static inline bool is_all_AMKS_running() {
    return g_car.front_right.state == AMK_STATE_RUNNING
        && g_car.front_left.state  == AMK_STATE_RUNNING
        && g_car.rear_left.state   == AMK_STATE_RUNNING
        && g_car.rear_right.state  == AMK_STATE_RUNNING;
}

static inline bool is_start_button_pressed() {
    if (can_data.start_button.is_pressed == false) {
        return false;
    }

    // "destructive read"
    can_data.start_button.is_pressed = false;
    return true;
}

static inline bool is_brakes_engaged() {
    static constexpr uint16_t BRAKES_ENGAGED_THRESHOLD = 10; // 10 %

    if (can_data.pedals.is_stale()) {
        return false;
    }

    return can_data.pedals.brake > BRAKES_ENGAGED_THRESHOLD;
}

static inline bool is_buzzing_time_elapsed() {
    // FSAE 2026 EV.9.7.2: The Ready to Drive Sound must be sounded continuously for minimum 1 second and maximum 3 seconds
    static constexpr uint32_t MIN_BUZZING_TIME_MS = 2500;

    return (OS_TICKS - g_car.buzzer_start_time >= MIN_BUZZING_TIME_MS);
}

static void update_brake_light() {
    static constexpr uint16_t BRAKE_LIGHT_ON_THRESHOLD  = 30; // 30 %
    static constexpr uint16_t BRAKE_LIGHT_OFF_THRESHOLD = 10; // 10 %

    if (can_data.pedals.regen > BRAKE_LIGHT_ON_THRESHOLD) {
        if (!g_car.brake_light) {
            g_car.brake_light = true;
        }
    } else if (can_data.pedals.regen < BRAKE_LIGHT_OFF_THRESHOLD) {
        if (g_car.brake_light) {
            g_car.brake_light = false;
        }
    }
}

static void update_tsal() {
    // FSAE 2026 EV.5.11.5: flash TSAL red either IMD or BMS are faulted
    if (is_latched(FAULT_ID_SDC1_IMD) || is_latched(FAULT_ID_SDC2_BMS)) { 
        g_car.tsal_green_enable = false;
        g_car.tsal_red_enable = true;
    } else {
        g_car.tsal_green_enable = true;
        g_car.tsal_red_enable = false;
    }
}

static void update_amks() {
    // iterate the AMK fsms
    AMK_periodic(&g_car.front_right);
    AMK_periodic(&g_car.front_left);
    AMK_periodic(&g_car.rear_left);
    AMK_periodic(&g_car.rear_right);
}

void vehicle_fsm_periodic(void) {
    // set default states
    g_car.current_state = g_car.next_state;
    g_car.next_state    = g_car.current_state; // explicit self loop
    g_car.brake_light   = false;
    g_car.buzzer_enable = false;

    // zero torque request by default
    g_torque_request.front_right = 0;
    g_torque_request.front_left  = 0;
    g_torque_request.rear_left   = 0;
    g_torque_request.rear_right  = 0;
    
    update_amks();
    update_brake_light();
    update_tsal();

    // update precharge status
    bool precharge_pin = PHAL_readGPIO(NOT_PRECHARGE_COMPLETE_PORT, NOT_PRECHARGE_COMPLETE_PIN);
    update_fault(FAULT_ID_PRECHARGE_INCOMPLETE, precharge_pin == true);
    // amks need a bool to point to for precharge status
    g_car.is_precharge_complete = is_clear(FAULT_ID_PRECHARGE_INCOMPLETE);

    // any SDCs latched 1-15 faults will cause a fatal state
    if (is_latched(FAULT_ID_SDC15_REAR_INTERLOCK)) {
        g_car.current_state = CAR_STATE_FATAL;
        g_car.next_state = CAR_STATE_FATAL;
    } else if (is_latched(FAULT_ID_SDC16_TSMS)) { // return to idle if TSMS is opened
        g_car.current_state = CAR_STATE_IDLE;
        g_car.next_state = CAR_STATE_IDLE;
    }

    switch (g_car.current_state) {
        case CAR_STATE_FATAL: {
            // nothing for now

            if (is_clear(FAULT_ID_SDC15_REAR_INTERLOCK)) {
                g_car.next_state = CAR_STATE_IDLE;
            }
            break;
        }
        case CAR_STATE_IDLE: {
            // do nothing for now

            if (is_clear(FAULT_ID_SDC16_TSMS)) { // TSMS is closed
                g_car.next_state = CAR_STATE_PRECHARGING;
            }
            break;
        }
        case CAR_STATE_PRECHARGING: {
            // do nothing for now

            if (is_clear(FAULT_ID_PRECHARGE_INCOMPLETE)) {
                g_car.next_state = CAR_STATE_ENERGIZED;
            }
            break;
        }
        case CAR_STATE_ENERGIZED: {
            // do nothing for now

            // FSAE 2026 EV.9.6.2: driver must engage the brakes and press a button to enter R2D
            bool is_driver_ready = is_start_button_pressed() && is_brakes_engaged();
            if (is_driver_ready && is_all_AMKS_running()) {
                g_car.buzzer_start_time = OS_TICKS;
                g_car.next_state = CAR_STATE_BUZZING;
            }
            break;
        }
        case CAR_STATE_BUZZING: {
            g_car.buzzer_enable = true;

            if (is_buzzing_time_elapsed()) {
                g_car.next_state = CAR_STATE_READY2DRIVE;
            }
            break;
        }
        case CAR_STATE_READY2DRIVE: {
            // FSAE 2026 EV.9.6.1: motors can only repond to apps in this state
            update_torque_request();

            if (is_start_button_pressed()) {
                g_car.next_state = CAR_STATE_ENERGIZED;
            }
            break;
        }
    }

    // flush the internal state
    AMK_set_torque(&g_car.front_right, g_torque_request.front_right);
    AMK_set_torque(&g_car.front_left,  g_torque_request.front_left);
    AMK_set_torque(&g_car.rear_left,   g_torque_request.rear_left);
    AMK_set_torque(&g_car.rear_right,  g_torque_request.rear_right);

    CAN_SEND_main_hb(g_car.current_state);

    PHAL_writeGPIO(BRAKE_LIGHT_PORT, BRAKE_LIGHT_PIN, g_car.brake_light);
    PHAL_writeGPIO(TSAL_GREEN_CTRL_PORT, TSAL_GREEN_CTRL_PIN, g_car.tsal_green_enable);
    PHAL_writeGPIO(TSAL_RED_CTRL_PORT, TSAL_RED_CTRL_PIN, g_car.tsal_red_enable);
    PHAL_writeGPIO(BUZZER_PORT, BUZZER_PIN, g_car.buzzer_enable);
}