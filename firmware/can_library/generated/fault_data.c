/**
 * @file fault_data.c
 * @brief Auto-generated implementation of the system-wide fault library.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/generated/fault_data.h"
#include "can_library/generated/can_router.h"

fault_t faults[TOTAL_NUM_FAULTS] = {
    // --- A_BOX ---
    [FAULT_ID_PACK_OVERTEMP] = {
        .start_time_ms = 0,
        .max_value = 60.0f,
        .min_value = 0.0f,
        .latch_time_ms = 500,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_PACK_WARM] = {
        .start_time_ms = 0,
        .max_value = 50.0f,
        .min_value = 0.0f,
        .latch_time_ms = 2000,
        .unlatch_time_ms = 5000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    [FAULT_ID_PACK_COLD] = {
        .start_time_ms = 0,
        .max_value = 10.0f,
        .min_value = 0.0f,
        .latch_time_ms = 2000,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_IMD] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 2000,
        .unlatch_time_ms = 1000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_BMS_DISCONNECTED] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 250,
        .unlatch_time_ms = 1000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_CELL_UNDERVOLTAGE] = {
        .start_time_ms = 0,
        .max_value = 10.0f,
        .min_value = 2.7f,
        .latch_time_ms = 1000,
        .unlatch_time_ms = 5000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_CELL_OVERVOLTAGE] = {
        .start_time_ms = 0,
        .max_value = 4.2f,
        .min_value = 0.0f,
        .latch_time_ms = 1000,
        .unlatch_time_ms = 5000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_PACK_FULL] = {
        .start_time_ms = 0,
        .max_value = 470.0f,
        .min_value = 0.0f,
        .latch_time_ms = 10000,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    [FAULT_ID_PACK_EMPTY] = {
        .start_time_ms = 0,
        .max_value = 1000.0f,
        .min_value = 302.0f,
        .latch_time_ms = 10000,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    // --- DASHBOARD ---
    [FAULT_ID_APPS_WIRING_T1] = {
        .start_time_ms = 0,
        .max_value = 4095.0f,
        .min_value = 50.0f,
        .latch_time_ms = 100,
        .unlatch_time_ms = 1000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_APPS_WIRING_T2] = {
        .start_time_ms = 0,
        .max_value = 3650.0f,
        .min_value = 0.0f,
        .latch_time_ms = 100,
        .unlatch_time_ms = 1000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_APPS_IMPLAUSIBLE] = {
        .start_time_ms = 0,
        .max_value = 10.0f,
        .min_value = 0.0f,
        .latch_time_ms = 100,
        .unlatch_time_ms = 50,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_APPS_BRAKE] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 0,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    [FAULT_ID_BSE] = {
        .start_time_ms = 0,
        .max_value = 4000.0f,
        .min_value = 300.0f,
        .latch_time_ms = 100,
        .unlatch_time_ms = 50,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    // --- MAIN_MODULE ---
    [FAULT_ID_SDC1_IMD] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC2_BMS] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC3_BSPD] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC4_ECU_OK] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC5_BOTS] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC6_INERTIA] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC7_COCKPIT_ESTOP] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC9_FRONT_INTERLOCK] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC10_RIGHT_ESTOP] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC11_LEFT_ESTOP] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC12_MSD] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC13_E_METER] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC14_HVD] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC16_REAR_INTERLOCK] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC17_TSMS] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_SDC19_PRECHARGE_COMPLETE] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 32,
        .unlatch_time_ms = 100,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_FATAL
    },
    [FAULT_ID_PRECHARGE_INCOMPLETE] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 500,
        .unlatch_time_ms = 500,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    // --- PDU ---
    [FAULT_ID_SDC_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_SDC_LOOP_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_MAIN_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_DASH_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_DAQ_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_ABOX_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_HV_BOARD_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_TV_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_DL_FRONT_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_DL_REAR_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_AMK1_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_AMK2_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_PUMP1_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_PUMP2_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_HXFAN_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_FAN1_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_FAN2_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_FAN3_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_FAN4_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_FAN5_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    [FAULT_ID_EMETER_SWITCH_FAULT] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 50,
        .unlatch_time_ms = 10000,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_ERROR
    },
    // --- TORQUE_VECTOR ---
    [FAULT_ID_GPS_INVALID_FIX] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 10,
        .unlatch_time_ms = 10,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    [FAULT_ID_GPS_WEAK_FIX] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 10,
        .unlatch_time_ms = 10,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
    [FAULT_ID_GPS_INVALID_UTC] = {
        .start_time_ms = 0,
        .max_value = 1.0f,
        .min_value = 0.0f,
        .latch_time_ms = 10,
        .unlatch_time_ms = 10,
        .state = FAULT_STATE_LATCHED, // start latched to sync initial state across MCUs
        .priority = FAULT_PRIO_WARNING
    },
};

#ifdef HAS_FAULT_STRINGS
const char* const fault_strings[TOTAL_NUM_FAULTS] = {
    // --- A_BOX ---
    [FAULT_ID_PACK_OVERTEMP] = "Pack Temp > 60C",
    [FAULT_ID_PACK_WARM] = "Pack Temp > 50C",
    [FAULT_ID_PACK_COLD] = "Pack Temp < 10C",
    [FAULT_ID_IMD] = "IMD Isolation Fault",
    [FAULT_ID_BMS_DISCONNECTED] = "BMS Disconnected",
    [FAULT_ID_CELL_UNDERVOLTAGE] = "Min Cell Voltage < 2.7V",
    [FAULT_ID_CELL_OVERVOLTAGE] = "Max Cell Voltage > 4.2V",
    [FAULT_ID_PACK_FULL] = "Pack Full",
    [FAULT_ID_PACK_EMPTY] = "Pack Empty",
    // --- DASHBOARD ---
    [FAULT_ID_APPS_WIRING_T1] = "APPS Wiring Fail T1",
    [FAULT_ID_APPS_WIRING_T2] = "APPS Wiring Fail T2",
    [FAULT_ID_APPS_IMPLAUSIBLE] = "APPS Implaus Detected",
    [FAULT_ID_APPS_BRAKE] = "Release Both Pedals",
    [FAULT_ID_BSE] = "Brake pressure disconnected",
    // --- MAIN_MODULE ---
    [FAULT_ID_SDC1_IMD] = "SDC1 (IMD) is open",
    [FAULT_ID_SDC2_BMS] = "SDC2 (BMS) is open",
    [FAULT_ID_SDC3_BSPD] = "SDC3 (BSPD) is open",
    [FAULT_ID_SDC4_ECU_OK] = "SDC4 (ECU_OK) is open",
    [FAULT_ID_SDC5_BOTS] = "SDC5 (BOTS) is open",
    [FAULT_ID_SDC6_INERTIA] = "SDC6 (INERTIA) is open",
    [FAULT_ID_SDC7_COCKPIT_ESTOP] = "SDC7 (COCKPIT_ESTOP) is open",
    [FAULT_ID_SDC9_FRONT_INTERLOCK] = "SDC9 (FRONT_INTERLOCK) is open",
    [FAULT_ID_SDC10_RIGHT_ESTOP] = "SDC10 (RIGHT_ESTOP) is open",
    [FAULT_ID_SDC11_LEFT_ESTOP] = "SDC11 (LEFT_ESTOP) is open",
    [FAULT_ID_SDC12_MSD] = "SDC12 (MSD) is open",
    [FAULT_ID_SDC13_E_METER] = "SDC13 (E_METER) is open",
    [FAULT_ID_SDC14_HVD] = "SDC14 (HVD) is open",
    [FAULT_ID_SDC16_REAR_INTERLOCK] = "SDC16 (REAR_INTERLOCK) is open",
    [FAULT_ID_SDC17_TSMS] = "SDC17 (TSMS) is open",
    [FAULT_ID_SDC19_PRECHARGE_COMPLETE] = "SDC19 (PRECHARGE_COMPLETE) is open",
    [FAULT_ID_PRECHARGE_INCOMPLETE] = "Precharge is not complete",
    // --- PDU ---
    [FAULT_ID_SDC_SWITCH_FAULT] = "SDC Switch Fault",
    [FAULT_ID_SDC_LOOP_SWITCH_FAULT] = "SDC Loop Switch Fault",
    [FAULT_ID_MAIN_SWITCH_FAULT] = "Main Switch Fault",
    [FAULT_ID_DASH_SWITCH_FAULT] = "Dash Switch Fault",
    [FAULT_ID_DAQ_SWITCH_FAULT] = "Daq Switch Fault",
    [FAULT_ID_ABOX_SWITCH_FAULT] = "Abox Switch Fault",
    [FAULT_ID_HV_BOARD_SWITCH_FAULT] = "Hv Board Switch Fault",
    [FAULT_ID_TV_SWITCH_FAULT] = "Tv Switch Fault",
    [FAULT_ID_DL_FRONT_SWITCH_FAULT] = "Dl Front Switch Fault",
    [FAULT_ID_DL_REAR_SWITCH_FAULT] = "Dl Rear Switch Fault",
    [FAULT_ID_AMK1_SWITCH_FAULT] = "AMK1 Switch Fault",
    [FAULT_ID_AMK2_SWITCH_FAULT] = "AMK2 Switch Fault",
    [FAULT_ID_PUMP1_SWITCH_FAULT] = "Pump1 Switch Fault",
    [FAULT_ID_PUMP2_SWITCH_FAULT] = "Pump2 Switch Fault",
    [FAULT_ID_HXFAN_SWITCH_FAULT] = "Hxfan Switch Fault",
    [FAULT_ID_FAN1_SWITCH_FAULT] = "Fan1 Switch Fault",
    [FAULT_ID_FAN2_SWITCH_FAULT] = "Fan2 Switch Fault",
    [FAULT_ID_FAN3_SWITCH_FAULT] = "Fan3 Switch Fault",
    [FAULT_ID_FAN4_SWITCH_FAULT] = "Fan4 Switch Fault",
    [FAULT_ID_FAN5_SWITCH_FAULT] = "Fan5 Switch Fault",
    [FAULT_ID_EMETER_SWITCH_FAULT] = "Emeter Switch Fault",
    // --- TORQUE_VECTOR ---
    [FAULT_ID_GPS_INVALID_FIX] = "Invalid GPS Fix",
    [FAULT_ID_GPS_WEAK_FIX] = "Weak GPS Fix",
    [FAULT_ID_GPS_INVALID_UTC] = "Invalid GPS UTC",
};
#endif

// --- A_BOX Callbacks ---
#ifdef FAULT_LIB_ENABLED
#ifndef CAN_NODE_A_BOX
void a_box_fault_sync_CALLBACK() {
    faults[FAULT_ID_PACK_OVERTEMP].state = can_data.a_box_fault_sync.PACK_OVERTEMP ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PACK_WARM].state = can_data.a_box_fault_sync.PACK_WARM ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PACK_COLD].state = can_data.a_box_fault_sync.PACK_COLD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_IMD].state = can_data.a_box_fault_sync.IMD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_BMS_DISCONNECTED].state = can_data.a_box_fault_sync.BMS_DISCONNECTED ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_CELL_UNDERVOLTAGE].state = can_data.a_box_fault_sync.CELL_UNDERVOLTAGE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_CELL_OVERVOLTAGE].state = can_data.a_box_fault_sync.CELL_OVERVOLTAGE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PACK_FULL].state = can_data.a_box_fault_sync.PACK_FULL ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PACK_EMPTY].state = can_data.a_box_fault_sync.PACK_EMPTY ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
}

void a_box_fault_event_CALLBACK() {
    uint16_t idx = can_data.a_box_fault_event.idx;
    if (idx < TOTAL_NUM_FAULTS) {
        faults[idx].state = (can_data.a_box_fault_event.state != 0) ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    }
}
#else
void a_box_fault_sync_CALLBACK() { }
void a_box_fault_event_CALLBACK() { }
#endif
#endif

// --- DASHBOARD Callbacks ---
#ifdef FAULT_LIB_ENABLED
#ifndef CAN_NODE_DASHBOARD
void dashboard_fault_sync_CALLBACK() {
    faults[FAULT_ID_APPS_WIRING_T1].state = can_data.dashboard_fault_sync.APPS_wiring_t1 ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_APPS_WIRING_T2].state = can_data.dashboard_fault_sync.APPS_wiring_t2 ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_APPS_IMPLAUSIBLE].state = can_data.dashboard_fault_sync.APPS_IMPLAUSIBLE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_APPS_BRAKE].state = can_data.dashboard_fault_sync.APPS_BRAKE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_BSE].state = can_data.dashboard_fault_sync.BSE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
}

void dashboard_fault_event_CALLBACK() {
    uint16_t idx = can_data.dashboard_fault_event.idx;
    if (idx < TOTAL_NUM_FAULTS) {
        faults[idx].state = (can_data.dashboard_fault_event.state != 0) ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    }
}
#else
void dashboard_fault_sync_CALLBACK() { }
void dashboard_fault_event_CALLBACK() { }
#endif
#endif

// --- MAIN_MODULE Callbacks ---
#ifdef FAULT_LIB_ENABLED
#ifndef CAN_NODE_MAIN_MODULE
void main_module_fault_sync_CALLBACK() {
    faults[FAULT_ID_SDC1_IMD].state = can_data.main_module_fault_sync.SDC1_IMD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC2_BMS].state = can_data.main_module_fault_sync.SDC2_BMS ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC3_BSPD].state = can_data.main_module_fault_sync.SDC3_BSPD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC4_ECU_OK].state = can_data.main_module_fault_sync.SDC4_ECU_OK ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC5_BOTS].state = can_data.main_module_fault_sync.SDC5_BOTS ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC6_INERTIA].state = can_data.main_module_fault_sync.SDC6_INERTIA ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC7_COCKPIT_ESTOP].state = can_data.main_module_fault_sync.SDC7_COCKPIT_ESTOP ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC9_FRONT_INTERLOCK].state = can_data.main_module_fault_sync.SDC9_FRONT_INTERLOCK ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC10_RIGHT_ESTOP].state = can_data.main_module_fault_sync.SDC10_RIGHT_ESTOP ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC11_LEFT_ESTOP].state = can_data.main_module_fault_sync.SDC11_LEFT_ESTOP ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC12_MSD].state = can_data.main_module_fault_sync.SDC12_MSD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC13_E_METER].state = can_data.main_module_fault_sync.SDC13_E_METER ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC14_HVD].state = can_data.main_module_fault_sync.SDC14_HVD ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC16_REAR_INTERLOCK].state = can_data.main_module_fault_sync.SDC16_REAR_INTERLOCK ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC17_TSMS].state = can_data.main_module_fault_sync.SDC17_TSMS ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC19_PRECHARGE_COMPLETE].state = can_data.main_module_fault_sync.SDC19_PRECHARGE_COMPLETE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PRECHARGE_INCOMPLETE].state = can_data.main_module_fault_sync.PRECHARGE_INCOMPLETE ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
}

void main_module_fault_event_CALLBACK() {
    uint16_t idx = can_data.main_module_fault_event.idx;
    if (idx < TOTAL_NUM_FAULTS) {
        faults[idx].state = (can_data.main_module_fault_event.state != 0) ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    }
}
#else
void main_module_fault_sync_CALLBACK() { }
void main_module_fault_event_CALLBACK() { }
#endif
#endif

// --- PDU Callbacks ---
#ifdef FAULT_LIB_ENABLED
#ifndef CAN_NODE_PDU
void pdu_fault_sync_CALLBACK() {
    faults[FAULT_ID_SDC_SWITCH_FAULT].state = can_data.pdu_fault_sync.SDC_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_SDC_LOOP_SWITCH_FAULT].state = can_data.pdu_fault_sync.SDC_LOOP_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_MAIN_SWITCH_FAULT].state = can_data.pdu_fault_sync.MAIN_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_DASH_SWITCH_FAULT].state = can_data.pdu_fault_sync.DASH_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_DAQ_SWITCH_FAULT].state = can_data.pdu_fault_sync.DAQ_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_ABOX_SWITCH_FAULT].state = can_data.pdu_fault_sync.ABOX_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_HV_BOARD_SWITCH_FAULT].state = can_data.pdu_fault_sync.HV_BOARD_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_TV_SWITCH_FAULT].state = can_data.pdu_fault_sync.TV_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_DL_FRONT_SWITCH_FAULT].state = can_data.pdu_fault_sync.DL_FRONT_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_DL_REAR_SWITCH_FAULT].state = can_data.pdu_fault_sync.DL_REAR_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_AMK1_SWITCH_FAULT].state = can_data.pdu_fault_sync.AMK1_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_AMK2_SWITCH_FAULT].state = can_data.pdu_fault_sync.AMK2_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PUMP1_SWITCH_FAULT].state = can_data.pdu_fault_sync.PUMP1_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_PUMP2_SWITCH_FAULT].state = can_data.pdu_fault_sync.PUMP2_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_HXFAN_SWITCH_FAULT].state = can_data.pdu_fault_sync.HXFAN_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_FAN1_SWITCH_FAULT].state = can_data.pdu_fault_sync.FAN1_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_FAN2_SWITCH_FAULT].state = can_data.pdu_fault_sync.FAN2_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_FAN3_SWITCH_FAULT].state = can_data.pdu_fault_sync.FAN3_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_FAN4_SWITCH_FAULT].state = can_data.pdu_fault_sync.FAN4_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_FAN5_SWITCH_FAULT].state = can_data.pdu_fault_sync.FAN5_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_EMETER_SWITCH_FAULT].state = can_data.pdu_fault_sync.EMETER_SWITCH_FAULT ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
}

void pdu_fault_event_CALLBACK() {
    uint16_t idx = can_data.pdu_fault_event.idx;
    if (idx < TOTAL_NUM_FAULTS) {
        faults[idx].state = (can_data.pdu_fault_event.state != 0) ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    }
}
#else
void pdu_fault_sync_CALLBACK() { }
void pdu_fault_event_CALLBACK() { }
#endif
#endif

// --- TORQUE_VECTOR Callbacks ---
#ifdef FAULT_LIB_ENABLED
#ifndef CAN_NODE_TORQUE_VECTOR
void torque_vector_fault_sync_CALLBACK() {
    faults[FAULT_ID_GPS_INVALID_FIX].state = can_data.torque_vector_fault_sync.GPS_INVALID_FIX ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_GPS_WEAK_FIX].state = can_data.torque_vector_fault_sync.GPS_WEAK_FIX ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    faults[FAULT_ID_GPS_INVALID_UTC].state = can_data.torque_vector_fault_sync.GPS_INVALID_UTC ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
}

void torque_vector_fault_event_CALLBACK() {
    uint16_t idx = can_data.torque_vector_fault_event.idx;
    if (idx < TOTAL_NUM_FAULTS) {
        faults[idx].state = (can_data.torque_vector_fault_event.state != 0) ? FAULT_STATE_LATCHED : FAULT_STATE_CLEAR;
    }
}
#else
void torque_vector_fault_sync_CALLBACK() { }
void torque_vector_fault_event_CALLBACK() { }
#endif
#endif

// --- Transmission Helpers ---
void tx_fault_sync(void) {
#ifdef FAULT_LIB_ENABLED
#ifdef CAN_NODE_A_BOX
    CAN_SEND_a_box_fault_sync(
        is_latched(FAULT_ID_PACK_OVERTEMP),
        is_latched(FAULT_ID_PACK_WARM),
        is_latched(FAULT_ID_PACK_COLD),
        is_latched(FAULT_ID_IMD),
        is_latched(FAULT_ID_BMS_DISCONNECTED),
        is_latched(FAULT_ID_CELL_UNDERVOLTAGE),
        is_latched(FAULT_ID_CELL_OVERVOLTAGE),
        is_latched(FAULT_ID_PACK_FULL),
        is_latched(FAULT_ID_PACK_EMPTY)
    );
#endif
#ifdef CAN_NODE_DASHBOARD
    CAN_SEND_dashboard_fault_sync(
        is_latched(FAULT_ID_APPS_WIRING_T1),
        is_latched(FAULT_ID_APPS_WIRING_T2),
        is_latched(FAULT_ID_APPS_IMPLAUSIBLE),
        is_latched(FAULT_ID_APPS_BRAKE),
        is_latched(FAULT_ID_BSE)
    );
#endif
#ifdef CAN_NODE_MAIN_MODULE
    CAN_SEND_main_module_fault_sync(
        is_latched(FAULT_ID_SDC1_IMD),
        is_latched(FAULT_ID_SDC2_BMS),
        is_latched(FAULT_ID_SDC3_BSPD),
        is_latched(FAULT_ID_SDC4_ECU_OK),
        is_latched(FAULT_ID_SDC5_BOTS),
        is_latched(FAULT_ID_SDC6_INERTIA),
        is_latched(FAULT_ID_SDC7_COCKPIT_ESTOP),
        is_latched(FAULT_ID_SDC9_FRONT_INTERLOCK),
        is_latched(FAULT_ID_SDC10_RIGHT_ESTOP),
        is_latched(FAULT_ID_SDC11_LEFT_ESTOP),
        is_latched(FAULT_ID_SDC12_MSD),
        is_latched(FAULT_ID_SDC13_E_METER),
        is_latched(FAULT_ID_SDC14_HVD),
        is_latched(FAULT_ID_SDC16_REAR_INTERLOCK),
        is_latched(FAULT_ID_SDC17_TSMS),
        is_latched(FAULT_ID_SDC19_PRECHARGE_COMPLETE),
        is_latched(FAULT_ID_PRECHARGE_INCOMPLETE)
    );
#endif
#ifdef CAN_NODE_PDU
    CAN_SEND_pdu_fault_sync(
        is_latched(FAULT_ID_SDC_SWITCH_FAULT),
        is_latched(FAULT_ID_SDC_LOOP_SWITCH_FAULT),
        is_latched(FAULT_ID_MAIN_SWITCH_FAULT),
        is_latched(FAULT_ID_DASH_SWITCH_FAULT),
        is_latched(FAULT_ID_DAQ_SWITCH_FAULT),
        is_latched(FAULT_ID_ABOX_SWITCH_FAULT),
        is_latched(FAULT_ID_HV_BOARD_SWITCH_FAULT),
        is_latched(FAULT_ID_TV_SWITCH_FAULT),
        is_latched(FAULT_ID_DL_FRONT_SWITCH_FAULT),
        is_latched(FAULT_ID_DL_REAR_SWITCH_FAULT),
        is_latched(FAULT_ID_AMK1_SWITCH_FAULT),
        is_latched(FAULT_ID_AMK2_SWITCH_FAULT),
        is_latched(FAULT_ID_PUMP1_SWITCH_FAULT),
        is_latched(FAULT_ID_PUMP2_SWITCH_FAULT),
        is_latched(FAULT_ID_HXFAN_SWITCH_FAULT),
        is_latched(FAULT_ID_FAN1_SWITCH_FAULT),
        is_latched(FAULT_ID_FAN2_SWITCH_FAULT),
        is_latched(FAULT_ID_FAN3_SWITCH_FAULT),
        is_latched(FAULT_ID_FAN4_SWITCH_FAULT),
        is_latched(FAULT_ID_FAN5_SWITCH_FAULT),
        is_latched(FAULT_ID_EMETER_SWITCH_FAULT)
    );
#endif
#ifdef CAN_NODE_TORQUE_VECTOR
    CAN_SEND_torque_vector_fault_sync(
        is_latched(FAULT_ID_GPS_INVALID_FIX),
        is_latched(FAULT_ID_GPS_WEAK_FIX),
        is_latched(FAULT_ID_GPS_INVALID_UTC)
    );
#endif
#endif
}

void tx_fault_event(fault_id_t idx, uint16_t value) {
#ifdef FAULT_LIB_ENABLED
#ifdef CAN_NODE_A_BOX
    if (idx >= FAULT_ID_PACK_OVERTEMP && idx <= FAULT_ID_PACK_EMPTY) {
        CAN_SEND_a_box_fault_event(idx, (faults[idx].state == FAULT_STATE_LATCHED), value);
    }
#endif
#ifdef CAN_NODE_DASHBOARD
    if (idx >= FAULT_ID_APPS_WIRING_T1 && idx <= FAULT_ID_BSE) {
        CAN_SEND_dashboard_fault_event(idx, (faults[idx].state == FAULT_STATE_LATCHED), value);
    }
#endif
#ifdef CAN_NODE_MAIN_MODULE
    if (idx >= FAULT_ID_SDC1_IMD && idx <= FAULT_ID_PRECHARGE_INCOMPLETE) {
        CAN_SEND_main_module_fault_event(idx, (faults[idx].state == FAULT_STATE_LATCHED), value);
    }
#endif
#ifdef CAN_NODE_PDU
    if (idx >= FAULT_ID_SDC_SWITCH_FAULT && idx <= FAULT_ID_EMETER_SWITCH_FAULT) {
        CAN_SEND_pdu_fault_event(idx, (faults[idx].state == FAULT_STATE_LATCHED), value);
    }
#endif
#ifdef CAN_NODE_TORQUE_VECTOR
    if (idx >= FAULT_ID_GPS_INVALID_FIX && idx <= FAULT_ID_GPS_INVALID_UTC) {
        CAN_SEND_torque_vector_fault_event(idx, (faults[idx].state == FAULT_STATE_LATCHED), value);
    }
#endif
#else
    (void)idx; (void)value;
#endif
}
