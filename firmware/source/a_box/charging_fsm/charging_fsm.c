/**
 * @file charging_fsm.c
 * @brief Charger control state machine implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "charging_fsm.h"

#include "adbms.h"
#include "can_library/faults_common.h"
#include "can_library/generated/A_BOX.h"
#include "can_library/generated/can_types.h"

static constexpr uint16_t MAX_PACK_CHARGING_VOLTS = 540;
static constexpr uint16_t MAX_PACK_CHARGING_AMPS = 20;

charging_state_t charging_state = CHARGING_STATE_IDLE;
charging_state_t next_charging_state = CHARGING_STATE_IDLE;

uint16_t charge_command_volts = 0;
uint16_t charge_command_amps = 0;
bool charge_enable = false;

extern adbms_bms_t g_bms;

static inline bool is_daqapp_requesting_charge() {
    return !can_data.charge_request.is_stale() && can_data.charge_request.charge_enable;
}

static inline bool is_elcon_ready() {
    return !can_data.elcon_status.is_stale() && !can_data.elcon_status.startup_fail;
}

static inline void report_internal_state() {
    uint16_t scaled_charge_command_volts = (uint16_t)(charge_command_volts * PACK_COEFF_CHARGING_FSM_INTERNALS_VOLTAGE_LIMIT);
    uint16_t scaled_charge_command_amps = (uint16_t)(charge_command_amps * PACK_COEFF_CHARGING_FSM_INTERNALS_CURRENT_LIMIT);

    CAN_SEND_charging_fsm_internals(
        scaled_charge_command_volts,
        scaled_charge_command_amps,
        g_bms.is_balancing_enabled,
        charging_state
    );
}

static inline void update_charge_command() {
    if (can_data.charge_request.is_stale()) {
        charge_command_volts = 0;
        charge_command_amps = 0;
        charge_enable = false;
        return;
    }

    uint16_t requested_voltage = (uint16_t)(can_data.charge_request.charge_voltage * UNPACK_COEFF_CHARGE_REQUEST_CHARGE_VOLTAGE);
    uint16_t requested_current = (uint16_t)(can_data.charge_request.charge_current * UNPACK_COEFF_CHARGE_REQUEST_CHARGE_CURRENT);

    // cap the charge command to max values
    if (requested_voltage > MAX_PACK_CHARGING_VOLTS) {
        requested_voltage = MAX_PACK_CHARGING_VOLTS;
    }

    // cap the charge command to max values
    if (requested_current > MAX_PACK_CHARGING_AMPS) {
        requested_current = MAX_PACK_CHARGING_AMPS;
    }

    charge_command_volts = requested_voltage;
    charge_command_amps = requested_current;
    charge_enable = can_data.charge_request.charge_enable;
}

static inline void update_balance_command() {
    if (can_data.charge_request.is_stale()) {
        g_bms.is_balancing_enabled = false;
        return;
    }

    g_bms.is_balancing_enabled = can_data.charge_request.balance_enable;
}

static inline bool is_charging_permitted() {
    return is_clear(FAULT_ID_PACK_FULL)
        && is_clear(FAULT_ID_BMS_DISCONNECTED)
        && is_elcon_ready();
}

void charging_fsm_periodic(void) {
    // set default states
    charging_state = next_charging_state;
    next_charging_state = charging_state;

    charge_command_volts = 0;
    charge_command_amps = 0;
    charge_enable = false;
    update_balance_command();

    if (!is_charging_permitted()) {
        charging_state = CHARGING_STATE_IDLE;
        next_charging_state = CHARGING_STATE_IDLE;
    }

    switch (charging_state) {
        case CHARGING_STATE_IDLE: {

            if (is_charging_permitted()) {
                next_charging_state = CHARGING_STATE_READY2CHARGE;
            }
            break;
        }
        case CHARGING_STATE_READY2CHARGE: {

            if (is_daqapp_requesting_charge()) {
                next_charging_state = CHARGING_STATE_CHARGING;
            }
            break;
        }
        case CHARGING_STATE_CHARGING: {
            update_charge_command();

            if (!is_daqapp_requesting_charge()) {
                next_charging_state = CHARGING_STATE_READY2CHARGE;
            }
            break;
        }
    }

    uint16_t scaled_charge_command_volts = (uint16_t)(charge_command_volts * PACK_COEFF_ELCON_COMMAND_VOLTAGE_LIMIT);
    uint16_t scaled_charge_command_amps = (uint16_t)(charge_command_amps * PACK_COEFF_ELCON_COMMAND_CURRENT_LIMIT);

    CAN_SEND_elcon_command(
        scaled_charge_command_volts,
        scaled_charge_command_amps,
        !charge_enable
    );

    report_internal_state();
}
