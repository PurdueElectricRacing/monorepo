/**
 * @file vehicle_fsm.c
 * @brief Master vehicle state machine implementation
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "vehicle_fsm.h"

#include "can_library/faults_common.h"
#include "can_library/generated/MAIN_MODULE.h"
#include "common/phal_G4/gpio/gpio.h"
#include "main.h"
#include "powertrain.h"
#include "torque_controller.h"

// Global data structures
car_t g_car = {
    .current_state     = CAR_STATE_FATAL,
    .next_state        = CAR_STATE_FATAL,
    .buzzer_start_time = 0,
    .brake_light       = false,
    .tsal_green_enable = false,
    .tsal_red_enable   = false,
    .buzzer_enable     = false,
};

static_assert(VEHICLE_FSM_PERIOD_MS == POWERTRAIN_PERIOD_MS);

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

    return (xTaskGetTickCount() - g_car.buzzer_start_time >= MIN_BUZZING_TIME_MS);
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

void vehicle_fsm_periodic(void) {
    // set default states
    g_car.current_state = g_car.next_state;
    g_car.next_state    = g_car.current_state; // explicit self loop
    g_car.brake_light   = false;
    g_car.buzzer_enable = false;

    torque_request_t torque_request = {0};
    update_brake_light();
    update_tsal();

    // update precharge status
    bool precharge_pin = PHAL_GPIO_read(NOT_PRECHARGE_COMPLETE_PORT, NOT_PRECHARGE_COMPLETE_PIN);
    update_fault(FAULT_ID_PRECHARGE_INCOMPLETE, precharge_pin == true);

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
            if (is_driver_ready && is_powertrain_ready()) {
                g_car.buzzer_start_time = xTaskGetTickCount();
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
            torque_request = torque_controller_get_request();

            if (is_start_button_pressed()) {
                g_car.next_state = CAR_STATE_ENERGIZED;
            }
            break;
        }
    }

    powertrain_set_torque_request(torque_request);
    powertrain_periodic();

    CAN_SEND_main_hb(g_car.current_state);

    PHAL_GPIO_write(BRAKE_LIGHT_PORT, BRAKE_LIGHT_PIN, g_car.brake_light);
    PHAL_GPIO_write(TSAL_GREEN_CTRL_PORT, TSAL_GREEN_CTRL_PIN, g_car.tsal_green_enable);
    PHAL_GPIO_write(TSAL_RED_CTRL_PORT, TSAL_RED_CTRL_PIN, g_car.tsal_red_enable);
    PHAL_GPIO_write(BUZZER_PORT, BUZZER_PIN, g_car.buzzer_enable);
}
